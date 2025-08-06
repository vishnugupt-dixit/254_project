#ifndef __STAGE_HELPERS_H__
#define __STAGE_HELPERS_H__

#include <stdio.h>
#include "utils.h"
#include "pipeline.h"

/// EXECUTE STAGE HELPERS ///

/**
 * input  : idex_reg_t
 * output : uint32_t alu_control signal
 **/
uint32_t gen_alu_control(idex_reg_t idex_reg)
{
  uint32_t alu_control = 0;
  
  switch (idex_reg.opcode) {
    case 0x33: // R-type
      switch (idex_reg.funct3) {
        case 0x0: // add/sub/mul
          switch (idex_reg.funct7) {
            case 0x00: alu_control = 0x0; break; // add
            case 0x20: alu_control = 0x1; break; // sub  
            case 0x01: alu_control = 0x8; break; // mul
            default: alu_control = 0x0; break;
          }
          break;
        case 0x1: // sll/mulh
          switch (idex_reg.funct7) {
            case 0x00: alu_control = 0x2; break; // sll
            case 0x01: alu_control = 0x9; break; // mulh
            default: alu_control = 0x2; break;
          }
          break;
        case 0x2: // slt
          alu_control = 0x3;
          break;
        case 0x4: // xor/div
          switch (idex_reg.funct7) {
            case 0x00: alu_control = 0x4; break; // xor
            case 0x01: alu_control = 0xA; break; // div
            default: alu_control = 0x4; break;
          }
          break;
        case 0x5: // srl/sra
          switch (idex_reg.funct7) {
            case 0x00: alu_control = 0x5; break; // srl
            case 0x20: alu_control = 0x6; break; // sra
            default: alu_control = 0x5; break;
          }
          break;
        case 0x6: // or/rem
          switch (idex_reg.funct7) {
            case 0x00: alu_control = 0x7; break; // or
            case 0x01: alu_control = 0xB; break; // rem
            default: alu_control = 0x7; break;
          }
          break;
        case 0x7: // and
          alu_control = 0xC;
          break;
        default:
          alu_control = 0x0;
          break;
      }
      break;
      
    case 0x13: // I-type immediate operations
      switch (idex_reg.funct3) {
        case 0x0: alu_control = 0x0; break; // addi
        case 0x1: alu_control = 0x2; break; // slli
        case 0x2: alu_control = 0x3; break; // slti
        case 0x4: alu_control = 0x4; break; // xori
        case 0x5: // srli/srai
          if ((idex_reg.imm >> 5) == 0x20) {
            alu_control = 0x6; // srai
          } else {
            alu_control = 0x5; // srli
          }
          break;
        case 0x6: alu_control = 0x7; break; // ori
        case 0x7: alu_control = 0xC; break; // andi
        default: alu_control = 0x0; break;
      }
      break;
      
    case 0x03: // Load instructions
    case 0x23: // Store instructions
      alu_control = 0x0; // Add for address calculation
      break;
      
    case 0x37: // LUI
    case 0x6F: // JAL
      alu_control = 0x0; // Add
      break;
      
    default:
      alu_control = 0x0;
      break;
  }

  return alu_control;
}

/**
 * input  : alu_inp1, alu_inp2, alu_control
 * output : uint32_t alu_result
 **/
uint32_t execute_alu(uint32_t alu_inp1, uint32_t alu_inp2, uint32_t alu_control)
{
  uint32_t result;
  switch(alu_control){
    case 0x0: // add
      result = alu_inp1 + alu_inp2;
      break;
    case 0x1: // sub
      result = alu_inp1 - alu_inp2;
      break;
    case 0x2: // sll (shift left logical)
      result = alu_inp1 << (alu_inp2 & 0x1F);
      break;
    case 0x3: // slt (set less than)
      result = ((int32_t)alu_inp1 < (int32_t)alu_inp2) ? 1 : 0;
      break;
    case 0x4: // xor
      result = alu_inp1 ^ alu_inp2;
      break;
    case 0x5: // srl (shift right logical)
      result = alu_inp1 >> (alu_inp2 & 0x1F);
      break;
    case 0x6: // sra (shift right arithmetic)
      result = (uint32_t)((int32_t)alu_inp1 >> (alu_inp2 & 0x1F));
      break;
    case 0x7: // or
      result = alu_inp1 | alu_inp2;
      break;
    case 0x8: // mul
      result = alu_inp1 * alu_inp2;
      break;
    case 0x9: // mulh
      result = (uint32_t)(((int64_t)(int32_t)alu_inp1 * (int64_t)(int32_t)alu_inp2) >> 32);
      break;
    case 0xA: // div
      if (alu_inp2 == 0) {
        result = 0xFFFFFFFF; // Division by zero
      } else {
        result = (uint32_t)((int32_t)alu_inp1 / (int32_t)alu_inp2);
      }
      break;
    case 0xB: // rem
      if (alu_inp2 == 0) {
        result = alu_inp1; // Remainder with zero divisor
      } else {
        result = (uint32_t)((int32_t)alu_inp1 % (int32_t)alu_inp2);
      }
      break;
    case 0xC: // and
      result = alu_inp1 & alu_inp2;
      break;
    default:
      result = 0xBADCAFFE;
      break;
  };
  return result;
}

/// DECODE STAGE HELPERS ///

/**
 * input  : Instruction
 * output : uint32_t immediate value
 **/
uint32_t gen_imm(Instruction instruction)
{
  int imm_val = 0;
  
  switch(instruction.opcode) {
    case 0x13: // I-type (immediate operations)
    case 0x03: // I-type (load operations)
    case 0x73: // I-type (ecall)
      imm_val = sign_extend_number(instruction.itype.imm, 12);
      break;
      
    case 0x23: // S-type (store operations)
      imm_val = get_store_offset(instruction);
      break;
      
    case 0x63: // B-type (branch operations)
      imm_val = get_branch_offset(instruction);
      break;
      
    case 0x37: // U-type (LUI)
      imm_val = instruction.utype.imm;
      break;
      
    case 0x6F: // UJ-type (JAL)
      imm_val = get_jump_offset(instruction);
      break;
      
    case 0x33: // R-type
    default:
      imm_val = 0;
      break;
  };
  
  return imm_val;
}

/**
 * generates all the control logic that flows around in the pipeline
 * input  : Instruction
 * output : idex_reg_t
 **/
idex_reg_t gen_control(Instruction instruction)
{
  idex_reg_t idex_reg = {0};
  
  switch(instruction.opcode) {
    case 0x33:  // R-type
      // Control signals for R-type instructions
      break;
    case 0x13:  // I-type immediate
      // Control signals for I-type instructions
      break;
    case 0x03:  // Load instructions
      // Control signals for load instructions
      break;
    case 0x23:  // Store instructions
      // Control signals for store instructions
      break;
    case 0x63:  // Branch instructions
      // Control signals for branch instructions
      break;
    case 0x37:  // LUI
      // Control signals for LUI
      break;
    case 0x6F:  // JAL
      // Control signals for JAL
      break;
    default:  // Remaining opcodes
      break;
  }
  
  return idex_reg;
}

/// MEMORY STAGE HELPERS ///

/**
 * evaluates whether a branch must be taken
 * input  : <open to implementation>
 * output : bool
 **/
bool gen_branch(/*<args>*/)
{
  // For Milestone 1, we assume no hazards, so branches are handled differently
  // This will be implemented in Milestone 2
  return false;
}

/// PIPELINE FEATURES ///

/**
 * Task   : Sets the pipeline wires for the forwarding unit's control signals
 *           based on the pipeline register values.
 * input  : pipeline_regs_t*, pipeline_wires_t*
 * output : None
*/
void gen_forward(pipeline_regs_t* pregs_p, pipeline_wires_t* pwires_p)
{
  // Forwarding will be implemented in Milestone 2
}

/**
 * Task   : Sets the pipeline wires for the hazard unit's control signals
 *           based on the pipeline register values.
 * input  : pipeline_regs_t*, pipeline_wires_t*
 * output : None
*/
void detect_hazard(pipeline_regs_t* pregs_p, pipeline_wires_t* pwires_p, regfile_t* regfile_p)
{
  // Hazard detection will be implemented in Milestone 2
}

///////////////////////////////////////////////////////////////////////////////

/// RESERVED FOR PRINTING REGISTER TRACE AFTER EACH CLOCK CYCLE ///
void print_register_trace(regfile_t* regfile_p)
{
  // print
  for (uint8_t i = 0; i < 8; i++)       // 8 columns
  {
    for (uint8_t j = 0; j < 4; j++)     // of 4 registers each
    {
      printf("r%2d=%08x ", i * 4 + j, regfile_p->R[i * 4 + j]);
    }
    printf("\n");
  }
  printf("\n");
}

#endif // __STAGE_HELPERS_H__