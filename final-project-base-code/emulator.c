#include <stdio.h> // for stderr
#include <stdlib.h> // for exit()
#include "types.h"
#include "utils.h"
#include "riscv.h"

void execute_rtype(Instruction, Processor *);
void execute_itype_except_load(Instruction, Processor *);
void execute_branch(Instruction, Processor *);
void execute_jal(Instruction, Processor *);
void execute_load(Instruction, Processor *, Byte *);
void execute_store(Instruction, Processor *, Byte *);
void execute_ecall(Processor *, Byte *);
void execute_lui(Instruction, Processor *);

void execute_instruction(uint32_t instruction_bits, Processor *processor,Byte *memory) {    
    Instruction instruction = parse_instruction(instruction_bits);
    switch(instruction.opcode) {
        case 0x33:
            execute_rtype(instruction, processor);
            break;
        case 0x13:
            execute_itype_except_load(instruction, processor);
            break;
        case 0x73:
            execute_ecall(processor, memory);
            break;
        case 0x63:
            execute_branch(instruction, processor);
            break;
        case 0x6F:
            execute_jal(instruction, processor);
            break;
        case 0x23:
            execute_store(instruction, processor, memory);
            break;
        case 0x03:
            execute_load(instruction, processor, memory);
            break;
        case 0x37:
            execute_lui(instruction, processor);
            break;
        default: // undefined opcode
            handle_invalid_instruction(instruction);
            exit(-1);
            break;
    }
}

void execute_rtype(Instruction instruction, Processor *processor) {
    switch (instruction.rtype.funct3){
        case 0x0:
            switch (instruction.rtype.funct7) {
                case 0x0:
                    // Add
                    processor->R[instruction.rtype.rd] =
                        ((sWord)processor->R[instruction.rtype.rs1]) +
                        ((sWord)processor->R[instruction.rtype.rs2]);
                    break;
                case 0x1:
                    // Mul
                    processor->R[instruction.rtype.rd] =
                        ((sWord)processor->R[instruction.rtype.rs1]) *
                        ((sWord)processor->R[instruction.rtype.rs2]);
                    break;
                case 0x20:
                    // Sub
                    processor->R[instruction.rtype.rd] =
                        ((sWord)processor->R[instruction.rtype.rs1]) -
                        ((sWord)processor->R[instruction.rtype.rs2]);
                    break;
                default:
                    handle_invalid_instruction(instruction);
                    exit(-1);
                    break;
            }
            break;
        /* YOUR CODE HERE */

        case 0x1:
            switch (instruction.rtype.funct7){
                case 0x0:
                    processor->R[instruction.rtype.rd]=
                        ((sWord)processor->R[instruction.rtype.rs1])<<
                        ((sWord)processor->R[instruction.rtype.rs2]);
                    break;
                case 0x01:
                    processor->R[instruction.rtype.rd]=
                        (sWord)(((sDouble)processor->R[instruction.rtype.rs1])*
                        ((sDouble)processor->R[instruction.rtype.rs2]))>>32;
                    break;
                default:
                    handle_invalid_instruction(instruction);
                    exit(-1);
                    break;
            }
            break;


        case 0x2:
            switch (instruction.rtype.funct7){
                case 0x0:
                    processor->R[instruction.rtype.rd]=
                        (((sWord)processor->R[instruction.rtype.rs1])<
                        ((sWord)processor->R[instruction.rtype.rs2]))?1:0;
                    break;
                case 0x01:
                    processor->R[instruction.rtype.rd]=
                        (((sWord)processor->R[instruction.rtype.rs1])*
                        ((sWord)processor->R[instruction.rtype.rs2]))>>32;
                    break;
                default:
                    handle_invalid_instruction(instruction);
                    exit(-1);
                    break;
            }
            

        case 0x3:
            switch (instruction.rtype.funct7){
                case 0x0:
                    processor->R[instruction.rtype.rd]=zero_extend_number(
                        (((sWord)processor->R[instruction.rtype.rs1])<
                        ((sWord)processor->R[instruction.rtype.rs2]))?1:0,2);
                    break;
                case 0x01:
                    processor->R[instruction.rtype.rd]=
                        (((sWord)processor->R[instruction.rtype.rs1])*
                        ((sWord)processor->R[instruction.rtype.rs2]))>>32;
                    break;
                default:
                    handle_invalid_instruction(instruction);
                    exit(-1);
                    break;
            }
        
        case 0x4:
            switch (instruction.rtype.funct7){
                case 0x0:
                    processor->R[instruction.rtype.rd]=
                    (((sWord)processor->R[instruction.rtype.rs1])^
                    ((sWord)processor->R[instruction.rtype.rs2]));
                break;

                case 0x01:
                    processor->R[instruction.rtype.rd]=
                        (((sWord)processor->R[instruction.rtype.rs1])/
                        ((sWord)processor->R[instruction.rtype.rs2]));
                    break;
                default:
                    handle_invalid_instruction(instruction);
                    exit(-1);
                    break;
            }
        
        case 0x5:
            switch (instruction.rtype.funct7){
                case 0x0:
                    processor->R[instruction.rtype.rd]=
                        (((sWord)processor->R[instruction.rtype.rs1])>>
                        ((sWord)processor->R[instruction.rtype.rs2]));
                break;

                case 0x2:
                    processor->R[instruction.rtype.rd]= sign_extend_number(
                        (((sWord)processor->R[instruction.rtype.rs1])>>
                        ((sWord)processor->R[instruction.rtype.rs2])),32);
                break;

                case 0x01:
                    processor->R[instruction.rtype.rd]=
                        (((sWord)processor->R[instruction.rtype.rs1])/
                        ((sWord)processor->R[instruction.rtype.rs2]));
                    break;
                default:
                    handle_invalid_instruction(instruction);
                    exit(-1);
                    break;
            }



        case 0x6:
            switch (instruction.rtype.funct7){
                case 0x0:
                    processor->R[instruction.rtype.rd]=
                        (((sWord)processor->R[instruction.rtype.rs1])|
                        ((sWord)processor->R[instruction.rtype.rs2]));
                    break;

                case 0x01:
                    processor->R[instruction.rtype.rd]=
                        (((sWord)processor->R[instruction.rtype.rs1])%
                        ((sWord)processor->R[instruction.rtype.rs2]));
                    break;

                default:
                    handle_invalid_instruction(instruction);
                    exit(-1);
                    break;
            }


            
        case 0x7:
            switch (instruction.rtype.funct7){
                case 0x0:
                    processor->R[instruction.rtype.rd]=
                        (((sWord)processor->R[instruction.rtype.rs1])&
                        ((sWord)processor->R[instruction.rtype.rs2]));
                    break;

                case 0x01:
                    processor->R[instruction.rtype.rd]=
                        (((sWord)processor->R[instruction.rtype.rs1])%
                        ((sWord)processor->R[instruction.rtype.rs2]));
                    break;

                default:
                    handle_invalid_instruction(instruction);
                    exit(-1);
                    break;
            }


	/* deal with other cases */
        default:
            handle_invalid_instruction(instruction);
            exit(-1);
            break;
    }
    // update PC
    processor->PC += 4;
}

void execute_itype_except_load(Instruction instruction, Processor *processor) {
    switch (instruction.itype.funct3) {
        /* YOUR CODE HERE */
        case 0x0:
            processor->R[instruction.itype.rd]=
                (((sWord)processor->R[instruction.itype.rs1])+
                ((sWord)processor->R[instruction.itype.imm]));
                break;

        case 0x1:
            processor->R[instruction.itype.rd]=
                (((sWord)processor->R[instruction.itype.rs1])<<
                ((sWord)processor->R[instruction.itype.imm]));
                break;
                
        case 0x2:
            processor->R[instruction.itype.rd]=
                (((sWord)processor->R[instruction.itype.rs1])<
                ((sWord)processor->R[instruction.itype.rs1]))?1:0;
                break;
            
        case 0x3:
            processor->R[instruction.itype.rd]=zero_extend_number(
                (((sWord)processor->R[instruction.itype.rs1])<
                ((sWord)processor->R[instruction.itype.rs1]))?1:0,2);
                break;

        case 0x4: 
            processor->R[instruction.itype.rd]=
                (((sWord)processor->R[instruction.itype.rs1])^
                ((sWord)processor->R[instruction.itype.imm]));
                break;

        case 0x5:

            if (instruction.itype.imm=0x0)
            {
                processor->R[instruction.itype.rd]=
                (((sWord)processor->R[instruction.itype.rs1])>>
                ((sWord)processor->R[instruction.itype.imm]));
                break;
            }

            else if (instruction.itype.imm=0x2)
            {
                processor->R[instruction.itype.rd]= sign_extend_number(
                (((sWord)processor->R[instruction.itype.rs1])>>
                ((sWord)processor->R[instruction.itype.imm])),5);
                break;
            }
        
        case 0x6:
            processor->R[instruction.itype.rd]=
            (((sWord)processor->R[instruction.itype.rs1])|
            ((sWord)processor->R[instruction.itype.imm]));
            break;
        
        case 0x7:
            processor->R[instruction.itype.rd]=
            (((sWord)processor->R[instruction.itype.rs1])&
            ((sWord)processor->R[instruction.itype.imm]));
            break;
        
            
        default:
            handle_invalid_instruction(instruction);
            break;
    }
}

void execute_ecall(Processor *p, Byte *memory) {
    Register i;
    
    // syscall number is given by a0 (x10)
    // argument is given by a1
    switch(p->R[10]) {
        case 1: // print an integer
            printf("%d",p->R[11]);
            p->PC += 4;
            break;
        case 4: // print a string
            for(i=p->R[11];i<MEMORY_SPACE && load(memory,i,LENGTH_BYTE);i++) {
                printf("%c",load(memory,i,LENGTH_BYTE));
            }
            p->PC += 4;
            break;
        case 10: // exit
            printf("exiting the simulator\n");
            exit(0);
            break;
        case 11: // print a character
            printf("%c",p->R[11]);
            p->PC += 4;
            break;
        default: // undefined ecall
            printf("Illegal ecall number %d\n", p->R[10]);
            exit(-1);
            break;
    }
}

void execute_branch(Instruction instruction, Processor *processor) {
    switch (instruction.sbtype.funct3) {
        /* YOUR CODE HERE */
        case 0x0:
            if (processor->R[instruction.sbtype.rs1] == processor->R[instruction.sbtype.rs2]){
                processor->PC += get_branch_offset(instruction);
            }
        break;

        case 0x1:
            if (processor->R[instruction.sbtype.rs1] != processor->R[instruction.sbtype.rs2]){
                processor->PC += get_branch_offset(instruction);
            }
        break;

        case 0x4:
            if (processor->R[instruction.sbtype.rs1] < processor->R[instruction.sbtype.rs2]){
                processor->PC += get_branch_offset(instruction);
            }
        break;

        case 0x5:
            if (processor->R[instruction.sbtype.rs1] >= processor->R[instruction.sbtype.rs2]){
                processor->PC += get_branch_offset(instruction);
            }
        break;

        case 0x6:
            if (processor->R[instruction.sbtype.rs1] < processor->R[instruction.sbtype.rs2]){
                processor->PC += get_branch_offset(instruction);
            }
        break;

        case 0x7:
            if (processor->R[instruction.sbtype.rs1] >= processor->R[instruction.sbtype.rs2]){
                processor->PC += get_branch_offset(instruction);
            }
        break;


        default:
            handle_invalid_instruction(instruction);
            exit(-1);
            break;
    }
}

void execute_load(Instruction instruction, Processor *processor, Byte *memory) {
    Address address = processor->R[instruction.itype.rs1] + sign_extend_number(instruction.itype.imm, 12); 
     /* YOUR CODE HERE */
    switch (instruction.itype.funct3) {
        // lb
        case 0x0:
        processor->R[instruction.itype.rd] =
            sign_extend_number(load(memory, address, LENGTH_BYTE), 8);
        break;

        // lh
        case 0x1:
        processor->R[instruction.itype.rd] =
            sign_extend_number(load(memory, address, LENGTH_HALF_WORD), 16);
        break;

        // lw
        case 0x2:
        processor->R[instruction.itype.rd] =
            load(memory, address, LENGTH_WORD);
         break; 

        default:
            handle_invalid_instruction(instruction);
            break;
    }

    processor->PC += 4;
}


void execute_store(Instruction instruction, Processor *processor, Byte *memory) {
    Address address = processor->R[instruction.stype.rs1] + sign_extend_number(instruction.stype.imm5, 12);
    switch (instruction.stype.funct3) {
        /* YOUR CODE HERE */
        // sb
        case 0x0:
            store(memory, address, LENGTH_BYTE, processor->R[instruction.stype.rs2]);
        break;

        // sh
        case 0x1:
            store(memory, address, LENGTH_HALF_WORD, processor->R[instruction.stype.rs2]);
        break;

        // sw
        case 0x2:
            store(memory, address, LENGTH_WORD, processor->R[instruction.stype.rs2]);
         break; 

        default:
            handle_invalid_instruction(instruction);
            exit(-1);
            break;
    }
    processor->PC += 4;
}

void execute_jal(Instruction instruction, Processor *processor) {
    /* YOUR CODE HERE */
    processor->R[instruction.ujtype.rd] = processor->PC +4; 
    
    processor->PC = processor->PC + sign_extend_number(instruction.ujtype.imm, 21);
}

void execute_lui(Instruction instruction, Processor *processor) {
    /* YOUR CODE HERE */
    processor->R[instruction.utype.rd] = instruction.utype.imm << 12;
    processor->PC +=4;
}

void store(Byte *memory, Address address, Alignment alignment, Word value) {
    /* YOUR CODE HERE */
    if (alignment == LENGTH_BYTE) {
        memory[address] = (Byte)(value & 0xFF); // stores the lowest byte
    } else if (alignment == LENGTH_HALF_WORD) {
        memory[address] = (Byte)(value & 0xFF); // low byte 
        memory[address + 1 ] = (Byte)((value >> 8)& 0xFF); //high byte 
    } else if (alignment == LENGTH_WORD) { 
         memory[address] = (Byte)(value & 0xFF); // LSB byte0
         memory[address + 1 ] = (Byte)((value >> 8)& 0xFF); // byte 1
         memory[address + 2 ] = (Byte)((value >> 16)& 0xFF); // byte 2 
         memory[address + 3 ] = (Byte)((value >> 24)& 0xFF); // MSB byte 3
    } else {
        printf("Error: Unrecognized alignment %d\n", alignment);
        exit(-1);
    }
}

Word load(Byte *memory, Address address, Alignment alignment) {
    if(alignment == LENGTH_BYTE) {
        return memory[address];
    } else if(alignment == LENGTH_HALF_WORD) {
        return (memory[address+1] << 8) + memory[address];
    } else if(alignment == LENGTH_WORD) {
        return (memory[address+3] << 24) + (memory[address+2] << 16)
               + (memory[address+1] << 8) + memory[address];
    } else {
        printf("Error: Unrecognized alignment %d\n", alignment);
        exit(-1);
    }
}