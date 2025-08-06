#ifndef __STAGE_HELPERS_H__
#define __STAGE_HELPERS_H__

#include <stdio.h>
#include "utils.h"
#include "pipeline.h"

/// EXECUTE STAGE HELPERS ///

uint32_t gen_alu_control(idex_reg_t idex_reg)
{
    uint32_t alu_control = 0;
    uint8_t opcode = idex_reg.instr.opcode;
    uint8_t funct3 = idex_reg.instr.itype.funct3;
    uint8_t funct7 = idex_reg.instr.rtype.funct7;

    switch (opcode) {
        case 0x33: // R-type
            switch (funct3) {
                case 0x0: alu_control = (funct7 == 0x20) ? 0x1 : 0x0; break; // SUB/ADD
                case 0x1: alu_control = 0x2; break; // SLL
                case 0x2: alu_control = 0x3; break; // SLT
                case 0x3: alu_control = 0x4; break; // SLTU
                case 0x4: alu_control = 0x5; break; // XOR
                case 0x5: alu_control = (funct7 == 0x20) ? 0x7 : 0x6; break; // SRA/SRL
                case 0x6: alu_control = 0x8; break; // OR
                case 0x7: alu_control = 0x9; break; // AND
                default: alu_control = 0x0; break;
            }
            break;
            
        case 0x13: // I-type
            switch (funct3) {
                case 0x0: alu_control = 0x0; break; // ADDI
                case 0x1: alu_control = 0x2; break; // SLLI
                case 0x2: alu_control = 0x3; break; // SLTI
                case 0x3: alu_control = 0x4; break; // SLTIU
                case 0x4: alu_control = 0x5; break; // XORI
                case 0x5: alu_control = (funct7 == 0x20) ? 0x7 : 0x6; break; // SRAI/SRLI
                case 0x6: alu_control = 0x8; break; // ORI
                case 0x7: alu_control = 0x9; break; // ANDI
                default: alu_control = 0x0; break;
            }
            break;
            
        case 0x63: // B-type
            switch(funct3){
                case 0x0:
                alu_control = 0xA; // BEQ comparison
                break;
                case 0x1:
                alu_control = 0xB; // BNE comparison
                break;
            }
            break;
            
        case 0x03: case 0x23: // Load/Store
            alu_control = 0x0; // Address calculation
            break;
            
        case 0x37: // LUI
            alu_control = 0xC; // Pass immediate
            break;
            
        default: 
            alu_control = 0xD; //BADCAFFEE
            break;
    }
    return alu_control;
}

uint32_t execute_alu(uint32_t alu_inp1, uint32_t alu_inp2, uint32_t alu_control)
{
    uint32_t result;
    switch(alu_control) {
        case 0x0: result = alu_inp1 + alu_inp2; break;  // ADD
        case 0x1: result = alu_inp1 - alu_inp2; break;  // SUB
        case 0x2: result = alu_inp1 << (alu_inp2 & 0x1F); break;  // SLL
        case 0x3: result = ((int32_t)alu_inp1 < (int32_t)alu_inp2) ? 1 : 0; break;  // SLT
        case 0x4: result = (alu_inp1 < alu_inp2) ? 1 : 0; break;  // SLTU
        case 0x5: result = alu_inp1 ^ alu_inp2; break;  // XOR
        case 0x6: result = alu_inp1 >> (alu_inp2 & 0x1F); break;  // SRL
        case 0x7: result = (int32_t)alu_inp1 >> (alu_inp2 & 0x1F); break;  // SRA
        case 0x8: result = alu_inp1 | alu_inp2; break;  // OR
        case 0x9: result = alu_inp1 & alu_inp2; break;  // AND
        case 0xA: result = !!(alu_inp1 - alu_inp2); break;  // BEQ comparison zero technically should be subtraction EQUAL = 0 NOT EQUAL = 1
        case 0xB: result = !(alu_inp1 ^ alu_inp2); break;// BNEQ comparison EQUAL = 1 NOT EQUAL =0
        case 0xC: result = alu_inp2; break;  // LUI (pass immediate)
        default: result = 0xBADCAFFE; break;
    }
    return result;
}

/// DECODE STAGE HELPERS ///

uint32_t gen_imm(Instruction instruction)
{
    int imm_val = 0;
    switch(instruction.opcode) {
        case 0x63: // B-type
            imm_val = get_branch_offset(instruction);
            break;
        case 0x03: case 0x13: case 0x73: // I-type
            imm_val = sign_extend_number(instruction.itype.imm, 12);
            if (instruction.itype.funct3 == 0x5 && ((imm_val >> 5) == 0x20)) //check if the shifting is correct
                imm_val = imm_val & 0x1F; 
            break;
        case 0x23: // S-type
            imm_val = get_store_offset(instruction);
            break;
        case 0x37: // U-type
            imm_val = instruction.utype.imm << 12;
            break;
        case 0x6f: // UJ-type
            imm_val = get_jump_offset(instruction);
            break;
        default:
            break;
    }
    return imm_val;
}

idex_reg_t gen_control(Instruction instruction)
{
    idex_reg_t idex_reg = {0};
    idex_reg.instr = instruction;
    
    switch(instruction.opcode) {
        case 0x33: // R-type
            idex_reg.EX_ALUSrc = false;
            idex_reg.WB_RegWrite = true;
            idex_reg.M_MemRead = false;
            idex_reg.M_MemWrite = false;
            idex_reg.WB_MemToReg = false;
            idex_reg.WB_WBSRC = false;
            idex_reg.M_Branch = false;
            idex_reg.M_JAL = false;
            idex_reg.EX_ALUOp = 0;
            break; 
            
        case 0x13: // I-type ALU
            idex_reg.EX_ALUSrc = true;
            idex_reg.WB_RegWrite = true;
            idex_reg.M_MemRead = false;
            idex_reg.M_MemWrite = false;
            idex_reg.WB_MemToReg = false;
            idex_reg.M_Branch = false;
            idex_reg.M_JAL = false;
            idex_reg.WB_WBSRC = false;
            idex_reg.EX_ALUOp = 1;
            break;
            
        case 0x03: // Load
            idex_reg.EX_ALUSrc = true;
            idex_reg.WB_RegWrite = true;
            idex_reg.M_MemRead = true;
            idex_reg.M_MemWrite = false;
            idex_reg.WB_MemToReg = true;
            idex_reg.M_Branch = false;
            idex_reg.M_JAL = false;
            idex_reg.WB_WBSRC = false;
            idex_reg.EX_ALUOp = 2;
            break;
            
        case 0x23: // Store
            idex_reg.EX_ALUSrc = true;
            idex_reg.WB_RegWrite = false;
            idex_reg.M_MemRead = false;
            idex_reg.M_MemWrite = true;
            idex_reg.M_Branch = false;
            idex_reg.M_JAL = false;
            idex_reg.WB_WBSRC = false;
            idex_reg.EX_ALUOp = 2;
            break;
            
        case 0x63: // Branch
            idex_reg.EX_ALUSrc = false;
            idex_reg.WB_RegWrite = false;
            idex_reg.M_Branch = true;
            idex_reg.M_MemRead = false;
            idex_reg.M_MemWrite = false;
            idex_reg.M_Branch = true;
            idex_reg.M_JAL = false;
            idex_reg.WB_WBSRC = false;
            idex_reg.EX_ALUOp = 3;
            break;
            
        case 0x37: case 0x17: // LUI/AUIPC
            idex_reg.EX_ALUSrc = true;
            idex_reg.WB_RegWrite = true;
            idex_reg.M_MemRead = false;
            idex_reg.M_MemWrite = false;
            idex_reg.WB_MemToReg = false;
            idex_reg.M_Branch = false;
            idex_reg.M_JAL = false;
            idex_reg.WB_WBSRC = false;
            idex_reg.EX_ALUOp = 4;
            break;
            
        case 0x6f: // JAL
            idex_reg.WB_RegWrite = true;
            idex_reg.WB_MemToReg = false;
            idex_reg.M_MemRead = false;
            idex_reg.M_MemWrite = false;
            idex_reg.M_Branch = false;
            idex_reg.M_JAL = true;
            idex_reg.WB_WBSRC = true;
            break;

        case 0x67:    //jalr
            idex_reg.WB_RegWrite = true;
            idex_reg.EX_ALUSrc = true;
            idex_reg.WB_MemToReg = false;
            idex_reg.M_MemRead = false;
            idex_reg.M_MemWrite = false;
            idex_reg.M_Branch = false;
            idex_reg.M_JAL = true;
            idex_reg.WB_WBSRC = true;
            break;
            
        default:
            break;
    }
    return idex_reg;
}

/// MEMORY STAGE HELPERS ///

bool gen_branch(exmem_reg_t exmem_reg)
{
    bool branch = exmem_reg.M_Branch;
    bool zero = exmem_reg.Zero;
    return (branch & zero);
}

/// PIPELINE FEATURES ///

void gen_forward(pipeline_regs_t* pregs_p, pipeline_wires_t* pwires_p)
{
    pwires_p->fwdA = FWD_REG;
    pwires_p->fwdB = FWD_REG;

    uint8_t rs1 = pregs_p->idex_preg.inp.instr.rtype.rs1;
    uint8_t rs2 = pregs_p->idex_preg.inp.instr.rtype.rs2;
    uint8_t exmem_rd = pregs_p->exmem_preg.out.instr.rtype.rd;
    uint8_t memwb_rd = pregs_p->memwb_preg.out.instr.rtype.rd;

    // EX/MEM → ID/EX
    if (pregs_p->exmem_preg.out.WB_RegWrite && exmem_rd != 0 && exmem_rd == rs1) {
        pwires_p->fwdA = FWD_EXMEM;
        fwd_exex_counter++;
    }
    if (pregs_p->exmem_preg.out.WB_RegWrite && exmem_rd != 0 && exmem_rd == rs2) {
        pwires_p->fwdB = FWD_EXMEM;
        fwd_exex_counter++;
    }

    // MEM/WB → ID/EX
    if (pregs_p->memwb_preg.out.WB_RegWrite && memwb_rd != 0 &&
        memwb_rd == rs1 && exmem_rd != rs1) {
        pwires_p->fwdA = FWD_MEMWB;
        fwd_exmem_counter++;
    }
    if (pregs_p->memwb_preg.out.WB_RegWrite && memwb_rd != 0 &&
        memwb_rd == rs2 && exmem_rd != rs2) {
        pwires_p->fwdB = FWD_MEMWB;
        fwd_exmem_counter++;
    }
}


void detect_hazard(pipeline_regs_t* pregs_p, pipeline_wires_t* pwires_p, regfile_t* regfile_p)
{
    pwires_p->stall = false;

    uint8_t id_rs1 = pregs_p->ifid_preg.out.instr.rtype.rs1;
    uint8_t id_rs2 = pregs_p->ifid_preg.out.instr.rtype.rs2;
    uint8_t ex_rd  = pregs_p->idex_preg.out.instr.rtype.rd;

    if (pregs_p->idex_preg.out.M_MemRead &&
        ((ex_rd == id_rs1) || (ex_rd == id_rs2)) && ex_rd != 0)
    {
        pwires_p->stall = true;
        stall_counter++;
    }
}

void print_register_trace(regfile_t* regfile_p)
{
    for (uint8_t i = 0; i < 8; i++) {
        for (uint8_t j = 0; j < 4; j++) {
            printf("r%2d=%08x ", i * 4 + j, regfile_p->R[i * 4 + j]);
        }
        printf("\n");
    }
    printf("\n");
}

#endif // __STAGE_HELPERS_H__