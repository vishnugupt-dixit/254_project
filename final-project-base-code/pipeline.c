// Complete implementation for pipeline.c - Milestone 1

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

simulator_config_t sim_config = {0};

///////////////////////////////////////////////////////////////////////////////

void bootstrap(pipeline_wires_t* pwires_p, pipeline_regs_t* pregs_p, regfile_t* regfile_p)
{
  // PC src must get the same value as the default PC value
  pwires_p->pc_src0 = regfile_p->PC;
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
  
  uint32_t instruction_bits = 0;
  
  // Fetch instruction from memory at PC
  if (regfile_p->PC < MEMORY_SPACE - 3) {
    instruction_bits = (memory_p[regfile_p->PC + 3] << 24) |
                      (memory_p[regfile_p->PC + 2] << 16) |
                      (memory_p[regfile_p->PC + 1] << 8) |
                      (memory_p[regfile_p->PC]);
  }

  // Handle NOP for uninitialized pipeline
  if (instruction_bits == 0) {
    instruction_bits = 0x00000013; // NOP (addi x0, x0, 0)
  }

  // Parse instruction
  ifid_reg.instr = parse_instruction(instruction_bits);
  ifid_reg.instr_addr = regfile_p->PC;
  ifid_reg.program_counter = regfile_p->PC;

  #ifdef DEBUG_CYCLE
  printf("[IF ]: Instruction [%08x]@[%08x]: ", instruction_bits, regfile_p->PC);
  decode_instruction(instruction_bits);
  #endif

  // Update PC (basic increment for Milestone 1 - no branches yet)
  regfile_p->PC += 4;

  return ifid_reg;
}

/**
 * STAGE  : stage_decode
 * output : idex_reg_t
 **/ 
idex_reg_t stage_decode(ifid_reg_t ifid_reg, pipeline_wires_t* pwires_p, regfile_t* regfile_p)
{
  idex_reg_t idex_reg = {0};
  
  // Copy instruction info
  idex_reg.instr = ifid_reg.instr;
  idex_reg.instr_addr = ifid_reg.instr_addr;
  idex_reg.program_counter = ifid_reg.program_counter;
  idex_reg.opcode = ifid_reg.instr.opcode;
  
  // Generate immediate based on instruction type
  idex_reg.imm = gen_imm(ifid_reg.instr);
  
  // Read register file
  switch (ifid_reg.instr.opcode) {
    case 0x33: // R-type
      idex_reg.read_data_1 = regfile_p->R[ifid_reg.instr.rtype.rs1];
      idex_reg.read_data_2 = regfile_p->R[ifid_reg.instr.rtype.rs2];
      idex_reg.funct3 = ifid_reg.instr.rtype.funct3;
      idex_reg.funct7 = ifid_reg.instr.rtype.funct7;
      break;
      
    case 0x13: // I-type (immediate operations)
      idex_reg.read_data_1 = regfile_p->R[ifid_reg.instr.itype.rs1];
      idex_reg.read_data_2 = 0; // Not used
      idex_reg.funct3 = ifid_reg.instr.itype.funct3;
      break;
      
    case 0x03: // Load instructions
      idex_reg.read_data_1 = regfile_p->R[ifid_reg.instr.itype.rs1];
      idex_reg.read_data_2 = 0;
      idex_reg.funct3 = ifid_reg.instr.itype.funct3;
      break;
      
    case 0x23: // Store instructions  
      idex_reg.read_data_1 = regfile_p->R[ifid_reg.instr.stype.rs1];
      idex_reg.read_data_2 = regfile_p->R[ifid_reg.instr.stype.rs2];
      idex_reg.funct3 = ifid_reg.instr.stype.funct3;
      break;
      
    case 0x63: // Branch instructions
      idex_reg.read_data_1 = regfile_p->R[ifid_reg.instr.sbtype.rs1];
      idex_reg.read_data_2 = regfile_p->R[ifid_reg.instr.sbtype.rs2];
      idex_reg.funct3 = ifid_reg.instr.sbtype.funct3;
      break;
      
    case 0x37: // LUI
      idex_reg.read_data_1 = 0;
      idex_reg.read_data_2 = 0;
      break;
      
    case 0x6F: // JAL
      idex_reg.read_data_1 = 0;
      idex_reg.read_data_2 = 0;
      break;
      
    default:
      idex_reg.read_data_1 = 0;
      idex_reg.read_data_2 = 0;
      break;
  }

  #ifdef DEBUG_CYCLE
  printf("[ID ]: Instruction [%08x]@[%08x]: ", ifid_reg.instr.bits, ifid_reg.instr_addr);
  decode_instruction(ifid_reg.instr.bits);
  #endif

  return idex_reg;
}

/**
 * STAGE  : stage_execute
 * output : exmem_reg_t
 **/ 
exmem_reg_t stage_execute(idex_reg_t idex_reg, pipeline_wires_t* pwires_p)
{
  exmem_reg_t exmem_reg = {0};
  
  // Copy instruction info
  exmem_reg.instr = idex_reg.instr;
  exmem_reg.instr_addr = idex_reg.instr_addr;
  exmem_reg.program_counter = idex_reg.program_counter;
  
  uint32_t alu_control = gen_alu_control(idex_reg);
  uint32_t alu_inp1 = idex_reg.read_data_1;
  uint32_t alu_inp2;
  
  // Select ALU input 2 based on instruction type
  switch (idex_reg.opcode) {
    case 0x33: // R-type - use register
      alu_inp2 = idex_reg.read_data_2;
      break;
    case 0x13: // I-type - use immediate
    case 0x03: // Load - use immediate
    case 0x23: // Store - use immediate
      alu_inp2 = idex_reg.imm;
      break;
    case 0x37: // LUI - special case
      alu_inp1 = 0;
      alu_inp2 = idex_reg.imm << 12;
      break;
    case 0x6F: // JAL - calculate return address
      alu_inp1 = idex_reg.program_counter;
      alu_inp2 = 4;
      break;
    default:
      alu_inp2 = idex_reg.imm;
      break;
  }
  
  // Execute ALU operation
  exmem_reg.addr = execute_alu(alu_inp1, alu_inp2, alu_control);
  
  // For store instructions, write_data is rs2
  if (idex_reg.opcode == 0x23) {
    exmem_reg.write_data = idex_reg.read_data_2;
  }

  #ifdef DEBUG_CYCLE
  printf("[EX ]: Instruction [%08x]@[%08x]: ", idex_reg.instr.bits, idex_reg.instr_addr);
  decode_instruction(idex_reg.instr.bits);
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
  
  // Copy instruction info
  memwb_reg.instr = exmem_reg.instr;
  memwb_reg.instr_addr = exmem_reg.instr_addr;
  memwb_reg.program_counter = exmem_reg.program_counter;
  memwb_reg.result_alu = exmem_reg.addr;
  
  // Handle memory operations
  switch (exmem_reg.instr.opcode) {
    case 0x03: // Load instructions
      switch (exmem_reg.instr.itype.funct3) {
        case 0x0: // lb
          memwb_reg.read_data = sign_extend_number(load(memory_p, exmem_reg.addr, LENGTH_BYTE), 8);
          break;
        case 0x1: // lh  
          memwb_reg.read_data = sign_extend_number(load(memory_p, exmem_reg.addr, LENGTH_HALF_WORD), 16);
          break;
        case 0x2: // lw
          memwb_reg.read_data = load(memory_p, exmem_reg.addr, LENGTH_WORD);
          break;
      }
      break;
      
    case 0x23: // Store instructions
      switch (exmem_reg.instr.stype.funct3) {
        case 0x0: // sb
          store(memory_p, exmem_reg.addr, LENGTH_BYTE, exmem_reg.write_data);
          break;
        case 0x1: // sh
          store(memory_p, exmem_reg.addr, LENGTH_HALF_WORD, exmem_reg.write_data);
          break;
        case 0x2: // sw
          store(memory_p, exmem_reg.addr, LENGTH_WORD, exmem_reg.write_data);
          break;
      }
      break;
      
    default:
      // No memory operation
      memwb_reg.read_data = 0;
      break;
  }

  #ifdef DEBUG_CYCLE
  printf("[MEM]: Instruction [%08x]@[%08x]: ", exmem_reg.instr.bits, exmem_reg.instr_addr);
  decode_instruction(exmem_reg.instr.bits);
  #endif

  return memwb_reg;
}

/**
 * STAGE  : stage_writeback
 * output : nothing - The state of the register file may be changed
 **/ 
void stage_writeback(memwb_reg_t memwb_reg, pipeline_wires_t* pwires_p, regfile_t* regfile_p)
{
  uint32_t write_reg = 0;
  uint32_t write_data = 0;
  bool reg_write = false;
  
  // Determine what to write back
  switch (memwb_reg.instr.opcode) {
    case 0x33: // R-type
      write_reg = memwb_reg.instr.rtype.rd;
      write_data = memwb_reg.result_alu;
      reg_write = (write_reg != 0); // Don't write to x0
      break;
      
    case 0x13: // I-type (immediate operations)
      write_reg = memwb_reg.instr.itype.rd;
      write_data = memwb_reg.result_alu;
      reg_write = (write_reg != 0);
      break;
      
    case 0x03: // Load instructions
      write_reg = memwb_reg.instr.itype.rd;
      write_data = memwb_reg.read_data;
      reg_write = (write_reg != 0);
      break;
      
    case 0x37: // LUI
      write_reg = memwb_reg.instr.utype.rd;
      write_data = memwb_reg.result_alu;
      reg_write = (write_reg != 0);
      break;
      
    case 0x6F: // JAL
      write_reg = memwb_reg.instr.ujtype.rd;
      write_data = memwb_reg.result_alu;
      reg_write = (write_reg != 0);
      break;
      
    case 0x23: // Store instructions
    case 0x63: // Branch instructions
    case 0x73: // ECALL
    default:
      reg_write = false;
      break;
  }
  
  // Write to register file
  if (reg_write) {
    regfile_p->R[write_reg] = write_data;
  }

  #ifdef DEBUG_CYCLE
  printf("[WB ]: Instruction [%08x]@[%08x]: ", memwb_reg.instr.bits, memwb_reg.instr_addr);
  decode_instruction(memwb_reg.instr.bits);
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

  /* Output               |    Stage      |       Inputs  */
  pregs_p->ifid_preg.inp  = stage_fetch     (pwires_p, regfile_p, memory_p);
  
  pregs_p->idex_preg.inp  = stage_decode    (pregs_p->ifid_preg.out, pwires_p, regfile_p);

  pregs_p->exmem_preg.inp = stage_execute   (pregs_p->idex_preg.out, pwires_p);

  pregs_p->memwb_preg.inp = stage_mem       (pregs_p->exmem_preg.out, pwires_p, memory_p, cache_p);

                            stage_writeback (pregs_p->memwb_preg.out, pwires_p, regfile_p);

  // update all the output registers for the next cycle from the input registers in the current cycle
  pregs_p->ifid_preg.out  = pregs_p->ifid_preg.inp;
  pregs_p->idex_preg.out  = pregs_p->idex_preg.inp;
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