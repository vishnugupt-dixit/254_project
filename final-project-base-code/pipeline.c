#include <stdbool.h>
#include "cache.h"
#include "riscv.h"
#include "types.h"
#include "utils.h"
#include "pipeline.h"
#include "stage_helpers.h"

uint64_t total_cycle_counter = 0;
uint64_t miss_count = 0;
uint64_t hit_count = 0;
uint64_t stall_counter = 0;
uint64_t branch_counter = 0;
uint64_t fwd_exex_counter = 0;
uint64_t fwd_exmem_counter = 0;
uint64_t mem_access_counter = 0;

simulator_config_t sim_config = {0};

///////////////////////////////////////////////////////////////////////////////

void bootstrap(pipeline_wires_t* pwires_p, pipeline_regs_t* pregs_p, regfile_t* regfile_p)
{
  // PC src must get the same value as the default PC value
  pwires_p->pc_src0 = regfile_p->PC;
  pwires_p->pcsrc   = false;

  pwires_p->fwdA    = FWD_REG;
  pwires_p->fwdB    = FWD_REG;
  pwires_p->stall   = false;
  pwires_p->flush   = false;
}

///////////////////////////
/// STAGE FUNCTIONALITY ///
///////////////////////////

/**
 * STAGE  : stage_fetch
 * output : ifid_reg_t
 **/ 
ifid_reg_t stage_fetch(pipeline_wires_t* pwires_p, regfile_t* regfile_p, Byte* memory_p)
{
  ifid_reg_t ifid_reg = {0};

  if (pwires_p->stall) {
  return (ifid_reg_t){0}; // Output nothing (NOP)
}

  
  // Handle PC source selection (branching)
  if (pwires_p->pcsrc == 1){
    regfile_p->PC = pwires_p->pc_src1;
    pwires_p->pcsrc = 0;
  }
  else{
    regfile_p->PC = pwires_p->pc_src0;
  }

  // Fetch instruction from memory
  uint32_t instruction_bits = *(uint32_t*)(memory_p + regfile_p->PC);
  ifid_reg.instr = parse_instruction(instruction_bits);

  #ifdef DEBUG_CYCLE
  printf("[IF ]: Instruction [%08x]@[%08x]: ", instruction_bits, regfile_p->PC);
  decode_instruction(instruction_bits);
  #endif
  
  ifid_reg.instr_addr = regfile_p->PC;
  pwires_p->pc_src0 = regfile_p->PC + 4;  // Next sequential PC
  
  return ifid_reg;
}

/**
 * STAGE  : stage_decode
 * output : idex_reg_t
 **/ 
idex_reg_t stage_decode(ifid_reg_t ifid_reg, pipeline_wires_t* pwires_p, regfile_t* regfile_p)
{
  idex_reg_t idex_reg = {0};
  
  // Generate control signals
  idex_reg = gen_control(ifid_reg.instr);
  
  // Read register file
  idex_reg.Read_Data_1 = regfile_p->R[ifid_reg.instr.rtype.rs1];
  idex_reg.Read_Data_2 = regfile_p->R[ifid_reg.instr.rtype.rs2];
  
  // Generate immediate
  idex_reg.imm_gen_out = gen_imm(ifid_reg.instr);
  
  // Pass through instruction address
  idex_reg.instr_addr = ifid_reg.instr_addr;
  
  uint32_t instruction_bits = idex_reg.instr.bits;
  
  #ifdef DEBUG_CYCLE
  printf("[ID ]: Instruction [%08x]@[%08x]: ", instruction_bits, idex_reg.instr_addr);
  decode_instruction(instruction_bits);
  #endif
  
  return idex_reg;
}

/**
 * STAGE  : stage_execute
 * output : exmem_reg_t
 **/ 
exmem_reg_t stage_execute(idex_reg_t idex_reg, pipeline_wires_t* pwires_p, pipeline_regs_t* pregs_p)
{
  exmem_reg_t exmem_reg = {0};
  
 
  exmem_reg.instr_addr = idex_reg.instr_addr;
  exmem_reg.instr = idex_reg.instr;
  

  exmem_reg.add_sum_output = idex_reg.imm_gen_out + idex_reg.instr_addr;
  

  uint32_t ALUcontrol = gen_alu_control(idex_reg);
  

  uint32_t alu_src1 = 0;
  uint32_t alu_src2 = 0;

  switch (pwires_p->fwdA) {
    case FWD_EXMEM:
      alu_src1 = pregs_p->exmem_preg.out.ALU_result;
      break;
    case FWD_MEMWB:
      alu_src1 = (pregs_p->memwb_preg.out.WB_MemToReg)
               ? pregs_p->memwb_preg.out.Read_Data
               : pregs_p->memwb_preg.out.ALU_result;
      break;
    default:
      alu_src1 = idex_reg.Read_Data_1;
      break;
  }

  switch (pwires_p->fwdB) {
    case FWD_EXMEM:
      alu_src2 = pregs_p->exmem_preg.out.ALU_result;
      break;
    case FWD_MEMWB:
     alu_src2 = (pregs_p->memwb_preg.out.WB_MemToReg)
               ? pregs_p->memwb_preg.out.Read_Data
                : pregs_p->memwb_preg.out.ALU_result;
    break;
    default:
    alu_src2 = idex_reg.Read_Data_2;
    break;
}


uint32_t alu_operand2 = (idex_reg.EX_ALUSrc == 1) ? idex_reg.imm_gen_out : alu_src2;


exmem_reg.ALU_result = execute_alu(alu_src1, alu_operand2, ALUcontrol);

  
  // Pass through Read_Data_2 for store operations
  exmem_reg.Read_Data_2 = idex_reg.Read_Data_2;
  
  // Pass through control signals
  exmem_reg.M_Branch = idex_reg.M_Branch;
  exmem_reg.M_JAL = idex_reg.M_JAL;
  exmem_reg.WB_WBSRC = idex_reg.WB_WBSRC;
  exmem_reg.M_MemRead = idex_reg.M_MemRead;
  exmem_reg.M_MemWrite = idex_reg.M_MemWrite;
  exmem_reg.WB_RegWrite = idex_reg.WB_RegWrite;
  exmem_reg.WB_MemToReg = idex_reg.WB_MemToReg;
  
  // Set Zero flag
  exmem_reg.Zero = !exmem_reg.ALU_result;
  
  uint32_t instruction_bits = exmem_reg.instr.bits;
  
  #ifdef DEBUG_CYCLE
  printf("[EX ]: Instruction [%08x]@[%08x]: ", instruction_bits, exmem_reg.instr_addr);
  decode_instruction(instruction_bits);
  #endif
  
  return exmem_reg;
}

/**
 * STAGE  : stage_mem
 * output : memwb_reg_t
 **/ 
memwb_reg_t stage_mem(exmem_reg_t exmem_reg, pipeline_wires_t* pwires_p, Byte* memory_p, Cache* cache_p)
{
  memwb_reg_t memwb_reg = {0};
  
  // Pass through instruction and address
  memwb_reg.instr = exmem_reg.instr;
  memwb_reg.instr_addr = exmem_reg.instr_addr;
  memwb_reg.ALU_result = exmem_reg.ALU_result;
  
  // Handle memory read operations
  if (exmem_reg.M_MemRead) {
    if (sim_config.cache_en) {
      // Cache handling would go here for milestone 2
    } else {
      switch (exmem_reg.instr.itype.funct3) {
        case 0x0: // Load Byte
          memwb_reg.Read_Data = sign_extend_number(load(memory_p, exmem_reg.ALU_result, LENGTH_BYTE), 8);
          break;
        case 0x1: // Load Halfword
          memwb_reg.Read_Data = sign_extend_number(load(memory_p, exmem_reg.ALU_result, LENGTH_HALF_WORD), 16);
          break;
        case 0x2: // Load Word
          memwb_reg.Read_Data = load(memory_p, exmem_reg.ALU_result, LENGTH_WORD);
          break;
        default:
          printf("Invalid load instruction\n");
          break;
      }
    }
  }
  
  // Handle memory write operations
  if (exmem_reg.M_MemWrite) {
    if (sim_config.cache_en) {
      // Cache handling would go here for milestone 2
    } else {
      switch (exmem_reg.instr.stype.funct3) {
        case 0x0: // Store Byte
          store(memory_p, exmem_reg.ALU_result, LENGTH_BYTE, exmem_reg.Read_Data_2);
          break;
        case 0x1: // Store Halfword
          store(memory_p, exmem_reg.ALU_result, LENGTH_HALF_WORD, exmem_reg.Read_Data_2);
          break;
        case 0x2: // Store Word
          store(memory_p, exmem_reg.ALU_result, LENGTH_WORD, exmem_reg.Read_Data_2);
          break;
        default:
          printf("Invalid store instruction\n");
          break;
      }
    }
  }
  
  // Handle PC source selection for branches and jumps
  pwires_p->pcsrc = (exmem_reg.Zero & exmem_reg.M_Branch) | exmem_reg.M_JAL;
  pwires_p->pc_src1 = exmem_reg.add_sum_output;
  
  // Pass through control signals
  memwb_reg.WB_RegWrite = exmem_reg.WB_RegWrite;
  memwb_reg.WB_MemToReg = exmem_reg.WB_MemToReg;
  memwb_reg.WB_WBSRC = exmem_reg.WB_WBSRC;
  
  uint32_t instruction_bits = memwb_reg.instr.bits;
  
  #ifdef DEBUG_CYCLE
  printf("[MEM]: Instruction [%08x]@[%08x]: ", instruction_bits, memwb_reg.instr_addr);
  decode_instruction(instruction_bits);
  #endif
  
  return memwb_reg;
}

/**
 * STAGE  : stage_writeback
 * output : nothing - The state of the register file may be changed
 **/ 
void stage_writeback(memwb_reg_t memwb_reg, pipeline_wires_t* pwires_p, regfile_t* regfile_p)
{
  // Memory to register mux
  uint32_t muxout;
  if (memwb_reg.WB_MemToReg == 1){
    muxout = memwb_reg.Read_Data;
  }
  else{
    muxout = memwb_reg.ALU_result;
  }
  
  // Write back source mux (for JAL/JALR)
  uint32_t muxout2;
  if (memwb_reg.WB_WBSRC == 1){
    muxout2 = memwb_reg.instr_addr + 4;
  }
  else{
    muxout2 = muxout;
  }
  
  // Write to register file if RegWrite is enabled
  if (memwb_reg.WB_RegWrite == 1){
    regfile_p->R[memwb_reg.instr.rtype.rd] = muxout2;
  }
  
  // Ensure x0 is always 0
  regfile_p->R[0] = 0;
  
  uint32_t instruction_bits = memwb_reg.instr.bits;
  
  #ifdef DEBUG_CYCLE
  printf("[WB ]: Instruction [%08x]@[%08x]: ", instruction_bits, memwb_reg.instr_addr);
  decode_instruction(instruction_bits);
  #endif
}

///////////////////////////////////////////////////////////////////////////////

/** 
 * excite the pipeline with one clock cycle
 **/
void cycle_pipeline(regfile_t* regfile_p, Byte* memory_p, Cache* cache_p, pipeline_regs_t* pregs_p, pipeline_wires_t* pwires_p, bool* ecall_exit)
{
  #ifdef DEBUG_CYCLE
  printf("v==============");
  printf("Cycle Counter = %5ld", total_cycle_counter);
  printf("==============v\n\n");
  #endif

  // process each stage

  gen_forward(pregs_p, pwires_p);
  detect_hazard(pregs_p, pwires_p, regfile_p);

  /* Output               |    Stage      |       Inputs  */
  pregs_p->ifid_preg.inp  = stage_fetch     (pwires_p, regfile_p, memory_p);
  
  pregs_p->idex_preg.inp  = stage_decode    (pregs_p->ifid_preg.out, pwires_p, regfile_p);

  pregs_p->exmem_preg.inp = stage_execute   (pregs_p->idex_preg.out, pwires_p, pregs_p);

  pregs_p->memwb_preg.inp = stage_mem       (pregs_p->exmem_preg.out, pwires_p, memory_p, cache_p);

                            stage_writeback (pregs_p->memwb_preg.out, pwires_p, regfile_p);

  //control hazards
if (pwires_p->pcsrc == 1) {
    pregs_p->ifid_preg.out = (ifid_reg_t){0};
    pregs_p->idex_preg.out = (idex_reg_t){0};
    pregs_p->idex_preg.out.instr.bits = 0x00000013;  // NOP
    branch_counter++;
}


  // update all the output registers for the next cycle from the input registers in the current cycle
// Update IF/ID output (stalling prevents PC and IF/ID register updates)
if (!pwires_p->stall) {
    pregs_p->ifid_preg.out = pregs_p->ifid_preg.inp;
} else {
    // re-freeze IF/ID output — keep old instruction for one more cycle
    pregs_p->ifid_preg.out = pregs_p->ifid_preg.out;
}

// Update ID/EX
if (pwires_p->stall) {
    // insert bubble
    pregs_p->idex_preg.out = (idex_reg_t){0};
    pregs_p->idex_preg.out.instr.bits = 0x00000013;  // addi x0, x0, 0 (NOP)
} else {
    pregs_p->idex_preg.out = pregs_p->idex_preg.inp;
}

// EX/MEM and MEM/WB always advance
pregs_p->exmem_preg.out = pregs_p->exmem_preg.inp;
pregs_p->memwb_preg.out = pregs_p->memwb_preg.inp;


  /////////////////// NO CHANGES BELOW THIS ARE REQUIRED //////////////////////

  // increment the cycle
  total_cycle_counter++;

  #ifdef DEBUG_REG_TRACE
  print_register_trace(regfile_p);
  #endif

  /**
   * check ecall condition
   * To do this, the value stored in R[10] (a0 or x10) should be 10.
   * Hence, the ecall condition is checked by the existence of following
   * two instructions in sequence:
   * 1. <instr>  x10, <val1>, <val2> 
   * 2. ecall
   * 
   * The first instruction must write the value 10 to x10.
   * The second instruction is the ecall (opcode: 0x73)
   * 
   * The condition checks whether the R[10] value is 10 when the
   * `memwb_reg.instr.opcode` == 0x73 (to propagate the ecall)
   * 
   * If more functionality on ecall needs to be added, it can be done
   * by adding more conditions on the value of R[10]
   */
  if( (pregs_p->memwb_preg.out.instr.bits == 0x00000073) &&
      (regfile_p->R[10] == 10) )
  {
    *(ecall_exit) = true;
  }
}