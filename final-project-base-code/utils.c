#include "utils.h"
#include <stdio.h>
#include <stdlib.h>

/* Unpacks the 32-bit machine code instruction given into the correct
 * type within the instruction struct */
Instruction parse_instruction(uint32_t instruction_bits) {
  /* YOUR CODE HERE */
  Instruction instruction;
  // add x9, x20, x21   hex: 01 5A 04 B3, binary = 0000 0001 0101 1010 0000 0100 1011 0011
  // Opcode: 0110011 (0x33) Get the Opcode by &ing 0x1111111, bottom 7 bits
  instruction.opcode = instruction_bits & ((1U << 7) - 1);

  // Shift right to move to pointer to interpret next fields in instruction.
  instruction_bits >>= 7;

  switch (instruction.opcode) {
  // R-Type
  case 0x33:
    // instruction: 0000 0001 0101 1010 0000 0100 1, destination : 01001
    instruction.rtype.rd = instruction_bits & ((1U << 5) - 1);
    instruction_bits >>= 5;

    // instruction: 0000 0001 0101 1010 0000, func3 : 000
    instruction.rtype.funct3 = instruction_bits & ((1U << 3) - 1);
    instruction_bits >>= 3;

    // instruction: 0000 0001 0101 1010 0, src1: 10100
    instruction.rtype.rs1 = instruction_bits & ((1U << 5) - 1);
    instruction_bits >>= 5;

    // instruction: 0000 0001 0101, src2: 10101
    instruction.rtype.rs2 = instruction_bits & ((1U << 5) - 1);
    instruction_bits >>= 5;

    // funct7: 0000 000
    instruction.rtype.funct7 = instruction_bits & ((1U << 7) - 1);
    break;
  // cases for other types of instructions
  /* YOUR CODE HERE */

  case 0x03: //i-type (0000011)
    instruction.itype.rd = instruction_bits & ((1U<<5)-1);
    instruction_bits >>= 5;

    instruction.itype.funct3 = instruction_bits & ((1U << 3)-1);
    instruction_bits >>= 3;

    instruction.itype.rs1 = instruction_bits & ((1U << 5)-1);
    instruction_bits >>= 5;

    instruction.itype.imm = instruction_bits & ((1U << 11)-1);
    instruction_bits >>=11;
    break;

    case 0x13: //i-type (0010011)
    instruction.itype.rd = instruction_bits & ((1U<<5)-1);
    instruction_bits >>= 5;

    instruction.itype.funct3 = instruction_bits & ((1U << 3)-1);
    instruction_bits >>= 3;

    instruction.itype.rs1 = instruction_bits & ((1U << 5)-1);
    instruction_bits >>= 5;

    instruction.itype.imm = instruction_bits & ((1U << 11)-1);
    instruction_bits >>=11;
    break;

    case 0x73://(1110011)
    instruction.itype.rd = instruction_bits & ((1U<<5)-1);
    instruction_bits >>= 5;

    instruction.itype.funct3 = instruction_bits & ((1U << 3)-1);
    instruction_bits >>= 3;

    instruction.itype.rs1 = instruction_bits & ((1U << 5)-1);
    instruction_bits >>= 5;

    instruction.itype.imm = instruction_bits & ((1U << 11)-1);
    instruction_bits >>=11;
    break;

    case 0x23: //stype
    instruction.stype.imm5 = instruction_bits & ((1U << 5)-1);
    instruction_bits >>= 5;

    instruction.stype.funct3 = instruction_bits & ((1U << 3)-1);
    instruction_bits >>=3;

    instruction.stype.rs1 = instruction_bits & ((1U << 5)-1);
    instruction_bits >>= 5;

    instruction.stype.rs2 = instruction_bits & ((1U << 5) -1);
    instruction_bits >>= 5;

    instruction.stype.imm7 = instruction_bits & ((1U << 7)-1);
    instruction_bits >>= 7;
    break;

    case 0x63:
    instruction.sbtype.imm5 = instruction_bits & ((1U << 5)-1);
    instruction_bits >>= 5;

    instruction.sbtype.funct3 = instruction_bits & ((1U << 3)-1);
    instruction_bits >>=3;

    instruction.sbtype.rs1 = instruction_bits & ((1U << 5)-1);
    instruction_bits >>= 5;

    instruction.sbtype.rs2 = instruction_bits & ((1U << 5) -1);
    instruction_bits >>= 5;

    instruction.sbtype.imm7 = instruction_bits & ((1U << 7)-1);
    instruction_bits >>= 7;
    break;

    case 0x37:
    instruction.utype.rd = instruction_bits & ((1U << 5)-1);
    instruction_bits >>= 5;

    instruction.utype.imm = instruction_bits & ((1U << 20)-1);
    instruction_bits >>=20;

    break;

    case 0x6f:
    instruction.ujtype.rd = instruction_bits & ((1U <<5)-1);
    instruction_bits >>= 5;
    
    instruction.ujtype.imm = instruction_bits & ((1U << 11)-1);
    instruction_bits >>= 11;
    break;
    


  #ifndef TESTING
  default:
    exit(EXIT_FAILURE);
  #endif
  }
  return instruction;
}

/************************Helper functions************************/
/* Here, you will need to implement a few common helper functions, 
 * which you will call in other functions when parsing, printing, 
 * or executing the instructions. */

/* Sign extends the given field to a 32-bit integer where field is
 * interpreted an n-bit integer. */
int sign_extend_number(unsigned int field, unsigned int n) {
  if (field & (1U << (n - 1))) { //bitmask
    // If the sign bit is 1, extend with 1s
    return (int)(field | (~0U << n));
  } else {
    // If the sign bit is 0, it's already positive
    return (int)field;
  }
}


/* Return the number of bytes (from the current PC) to the branch label using
 * the given branch instruction */
int get_branch_offset(Instruction instruction) {
    uint32_t imm = 0;

    // SB-type immediate is spread out: [12|10:5|4:1|11] (13 bits, signed)
    imm |= ((instruction.sbtype.imm7 >> 6) & 0x1) << 12;     
    imm |= (instruction.sbtype.imm7 & 0x3F) << 5;     
    imm |= (instruction.sbtype.imm5 & 0x1) << 11;    
    imm |= ((instruction.sbtype.imm5 >> 1) & 0x1F) << 1;

    // Sign-extend 13-bit to 32-bit signed integer
    imm = sign_extend_number(imm, 13);

    return imm;
}


/* Returns the number of bytes (from the current PC) to the jump label using the
 * given jump instruction */



 
int get_jump_offset(Instruction instruction) {
    //int imm = 0x0;

    // These fields are already unpacked into the ujtype struct fields
    int imm = ((instruction.ujtype.imm >> 19) & 0x1 ) << 20 |      
      ((instruction.ujtype.imm >> 9) & 0x3FF) << 1 | 
      ((instruction.ujtype.imm >> 8) & 0x1) << 11 | 
      (instruction.ujtype.imm & 0xFF) << 12;  

    
    //imm = sign_extend_number(imm,21);

    return sign_extend_number(imm,21);
}







/* Returns the number of bytes (from the current PC) to the base address using the
 * given store instruction */
int get_store_offset(Instruction instruction) {
    int imm = 0;

    imm |= instruction.stype.imm5;             // bits [4:0]
    imm |= instruction.stype.imm7 << 5;        // bits [11:5]

    // Sign extend the 12-bit result
    imm = sign_extend_number(imm, 12);

    return imm;
}

/************************Helper functions************************/

void handle_invalid_instruction(Instruction instruction) {
  printf("Invalid Instruction: 0x%08x\n", instruction.bits);
}

void handle_invalid_read(Address address) {
  printf("Bad Read. Address: 0x%08x\n", address);
  exit(-1);
}

void handle_invalid_write(Address address) {
  printf("Bad Write. Address: 0x%08x\n", address);
  exit(-1);
}