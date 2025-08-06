#ifndef __PIPELINE_H__
#define __PIPELINE_H__

#include "config.h"
#include "types.h"
#include "cache.h"
#include "riscv.h"
#include <stdbool.h>

///////////////////////////////////////////////////////////////////////////////
/// Functionality
///////////////////////////////////////////////////////////////////////////////

extern simulator_config_t sim_config;
extern uint64_t miss_count;
extern uint64_t hit_count;
extern uint64_t total_cycle_counter;
extern uint64_t stall_counter;
extern uint64_t branch_counter;
extern uint64_t fwd_exex_counter;
extern uint64_t fwd_exmem_counter;

///////////////////////////////////////////////////////////////////////////////
/// RISC-V Pipeline Register Types
///////////////////////////////////////////////////////////////////////////////

typedef struct
{
  Instruction instr;
  uint32_t    instr_addr;
  /**
   * Add other fields here
   */
  uint32_t    program_counter;
  regfile_t   read_reg_1;
  regfile_t   read_reg_2;
  int imm;
    
}ifid_reg_t;

typedef struct
{
  Instruction instr;
  uint32_t    instr_addr;
  /**
   * Add other fields here
   */
  unsigned int   read_data_1;
  unsigned int   read_data_2;
  uint32_t    program_counter;
  unsigned int imm;
  unsigned int opcode;
  unsigned int funct3;
  unsigned int funct7;

uint32_t alu_op;
uint8_t  alu_src;
uint8_t  reg_write;
uint8_t  mem_read;
uint8_t  mem_write;
uint8_t  mem_to_reg;
uint8_t  branch; 
}idex_reg_t;

typedef struct
{
  Instruction instr;
  uint32_t    instr_addr;
  /**
   * Add other fields here
   */
  uint32_t    program_counter;
  unsigned int  addr;
  unsigned int write_data;
}exmem_reg_t;

typedef struct
{
  Instruction instr;
  uint32_t    instr_addr;
  /**
   * Add other fields here
   */
  uint32_t    program_counter;
  unsigned int read_data;
  unsigned int write_reg;
  unsigned int result_alu;
  unsigned int read_mem;
}memwb_reg_t;


///////////////////////////////////////////////////////////////////////////////
/// Register types with input and output variants for simulator
///////////////////////////////////////////////////////////////////////////////

typedef struct
{
  ifid_reg_t inp;
  ifid_reg_t out;
}ifid_reg_pair_t;

typedef struct
{
  idex_reg_t inp;
  idex_reg_t out;
}idex_reg_pair_t;

typedef struct
{
  exmem_reg_t inp;
  exmem_reg_t out;
}exmem_reg_pair_t;

typedef struct
{
  memwb_reg_t inp;
  memwb_reg_t out;
}memwb_reg_pair_t;

///////////////////////////////////////////////////////////////////////////////
/// Functional pipeline requirements
///////////////////////////////////////////////////////////////////////////////

typedef struct
{
  ifid_reg_pair_t  ifid_preg;
  idex_reg_pair_t  idex_preg;
  exmem_reg_pair_t exmem_preg;
  memwb_reg_pair_t memwb_preg;
}pipeline_regs_t;

typedef struct
{
  bool      pcsrc;
  uint32_t  pc_src0;
  uint32_t  pc_src1;
  /**
   * Add other fields here
   */

  //alu control 
  uint32_t alu_op;
  uint8_t alu_src;
  uint8_t branch;

  //memory control 
  uint8_t mem_read;
  uint8_t mem_write;


  //write back control 
  uint8_t reg_write;
  uint8_t mem_to_reg;


}pipeline_wires_t;


///////////////////////////////////////////////////////////////////////////////
/// Function definitions for different stages
///////////////////////////////////////////////////////////////////////////////

/**
 * output : ifid_reg_t
 **/ 
ifid_reg_t stage_fetch(pipeline_wires_t* pwires_p, regfile_t* regfile_p, Byte* memory_p);

/**
 * output : idex_reg_t
 **/ 
idex_reg_t stage_decode(ifid_reg_t ifid_reg, pipeline_wires_t* pwires_p, regfile_t* regfile_p);

/**
 * output : exmem_reg_t
 **/ 
exmem_reg_t stage_execute(idex_reg_t idex_reg, pipeline_wires_t* pwires_p);

/**
 * output : memwb_reg_t
 **/ 
memwb_reg_t stage_mem(exmem_reg_t exmem_reg, pipeline_wires_t* pwires_p, Byte* memory, Cache* cache_p);

/**
 * output : write_data
 **/ 
void stage_writeback(memwb_reg_t memwb_reg, pipeline_wires_t* pwires_p, regfile_t* regfile_p);

void cycle_pipeline(regfile_t* regfile_p, Byte* memory_p, Cache* cache_p, pipeline_regs_t* pregs_p, pipeline_wires_t* pwires_p, bool* ecall_exit);

void bootstrap(pipeline_wires_t* pwires_p, pipeline_regs_t* pregs_p, regfile_t* regfile_p);

#endif  // __PIPELINE_H__
