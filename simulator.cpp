#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include <cstdint>
#include <iomanip>

using namespace std;

class RISCVSimulator {
private:
    uint32_t reg[32];           // 32 general-purpose registers
    uint32_t pc;                // Program counter
    uint8_t memory[1 << 20];    // 1MB memory
    vector<uint32_t> instructions;
    bool running;
    
    // Instruction fields
    uint32_t opcode, rd, rs1, rs2, funct3, funct7;
    int32_t imm;
    
    void decodeInstruction(uint32_t inst) {
        opcode = inst & 0x7F;
        rd = (inst >> 7) & 0x1F;
        funct3 = (inst >> 12) & 0x7;
        rs1 = (inst >> 15) & 0x1F;
        rs2 = (inst >> 20) & 0x1F;
        funct7 = (inst >> 25) & 0x7F;
        
        // Decode immediate based on instruction type
        switch (opcode) {
            case 0x13: // I-type (ADDI, etc.)
            case 0x03: // Load
            case 0x67: // JALR
                imm = (int32_t)inst >> 20;
                break;
            case 0x23: // S-type (Store)
                imm = ((int32_t)(inst & 0xFE000000) >> 20) | ((inst >> 7) & 0x1F);
                break;
            case 0x63: // B-type (Branch)
                imm = ((int32_t)(inst & 0x80000000) >> 19) |
                      ((inst & 0x80) << 4) |
                      ((inst >> 20) & 0x7E0) |
                      ((inst >> 7) & 0x1E);
                break;
            case 0x37: // U-type (LUI)
            case 0x17: // AUIPC
                imm = inst & 0xFFFFF000;
                break;
            case 0x6F: // J-type (JAL)
                imm = ((int32_t)(inst & 0x80000000) >> 11) |
                      (inst & 0xFF000) |
                      ((inst >> 9) & 0x800) |
                      ((inst >> 20) & 0x7FE);
                break;
            default:
                imm = 0;
        }
    }
    
    void executeInstruction(uint32_t inst) {
        decodeInstruction(inst);
        
        switch (opcode) {
            case 0x33: // R-type
                executeRType();
                break;
            case 0x13: // I-type (arithmetic)
                executeIType();
                break;
            case 0x03: // Load
                executeLoad();
                break;
            case 0x23: // Store
                executeStore();
                break;
            case 0x63: // Branch
                executeBranch();
                return; // Don't increment PC
            case 0x6F: // JAL
                executeJAL();
                return;
            case 0x67: // JALR
                executeJALR();
                return;
            case 0x37: // LUI
                reg[rd] = imm;
                break;
            case 0x17: // AUIPC
                reg[rd] = pc + imm;
                break;
            case 0x73: // ECALL/EBREAK
                running = false;
                return;
            default:
                cerr << "Unknown opcode: 0x" << hex << opcode << endl;
                running = false;
                return;
        }
        
        pc += 4;
        reg[0] = 0; // x0 is always 0
    }
    
    void executeRType() {
        uint32_t result;
        switch (funct3) {
            case 0x0: // ADD/SUB
                if (funct7 == 0x00) {
                    result = reg[rs1] + reg[rs2];
                } else if (funct7 == 0x20) {
                    result = reg[rs1] - reg[rs2];
                }
                break;
            case 0x1: // SLL
                result = reg[rs1] << (reg[rs2] & 0x1F);
                break;
            case 0x2: // SLT
                result = ((int32_t)reg[rs1] < (int32_t)reg[rs2]) ? 1 : 0;
                break;
            case 0x3: // SLTU
                result = (reg[rs1] < reg[rs2]) ? 1 : 0;
                break;
            case 0x4: // XOR
                result = reg[rs1] ^ reg[rs2];
                break;
            case 0x5: // SRL/SRA
                if (funct7 == 0x00) {
                    result = reg[rs1] >> (reg[rs2] & 0x1F);
                } else if (funct7 == 0x20) {
                    result = (int32_t)reg[rs1] >> (reg[rs2] & 0x1F);
                }
                break;
            case 0x6: // OR
                result = reg[rs1] | reg[rs2];
                break;
            case 0x7: // AND
                result = reg[rs1] & reg[rs2];
                break;
        }
        reg[rd] = result;
    }
    
    void executeIType() {
        uint32_t result;
        switch (funct3) {
            case 0x0: // ADDI
                result = reg[rs1] + imm;
                break;
            case 0x1: // SLLI
                result = reg[rs1] << (imm & 0x1F);
                break;
            case 0x2: // SLTI
                result = ((int32_t)reg[rs1] < imm) ? 1 : 0;
                break;
            case 0x3: // SLTIU
                result = (reg[rs1] < (uint32_t)imm) ? 1 : 0;
                break;
            case 0x4: // XORI
                result = reg[rs1] ^ imm;
                break;
            case 0x5: // SRLI/SRAI
                if ((imm & 0x400) == 0) {
                    result = reg[rs1] >> (imm & 0x1F);
                } else {
                    result = (int32_t)reg[rs1] >> (imm & 0x1F);
                }
                break;
            case 0x6: // ORI
                result = reg[rs1] | imm;
                break;
            case 0x7: // ANDI
                result = reg[rs1] & imm;
                break;
        }
        reg[rd] = result;
    }
    
    void executeLoad() {
        uint32_t addr = reg[rs1] + imm;
        switch (funct3) {
            case 0x0: // LB
                reg[rd] = (int32_t)(int8_t)memory[addr];
                break;
            case 0x1: // LH
                reg[rd] = (int32_t)(int16_t)(memory[addr] | (memory[addr+1] << 8));
                break;
            case 0x2: // LW
                reg[rd] = memory[addr] | (memory[addr+1] << 8) | 
                         (memory[addr+2] << 16) | (memory[addr+3] << 24);
                break;
            case 0x4: // LBU
                reg[rd] = memory[addr];
                break;
            case 0x5: // LHU
                reg[rd] = memory[addr] | (memory[addr+1] << 8);
                break;
        }
    }
    
    void executeStore() {
        uint32_t addr = reg[rs1] + imm;
        switch (funct3) {
            case 0x0: // SB
                memory[addr] = reg[rs2] & 0xFF;
                break;
            case 0x1: // SH
                memory[addr] = reg[rs2] & 0xFF;
                memory[addr+1] = (reg[rs2] >> 8) & 0xFF;
                break;
            case 0x2: // SW
                memory[addr] = reg[rs2] & 0xFF;
                memory[addr+1] = (reg[rs2] >> 8) & 0xFF;
                memory[addr+2] = (reg[rs2] >> 16) & 0xFF;
                memory[addr+3] = (reg[rs2] >> 24) & 0xFF;
                break;
        }
    }
    
    void executeBranch() {
        bool taken = false;
        switch (funct3) {
            case 0x0: // BEQ
                taken = (reg[rs1] == reg[rs2]);
                break;
            case 0x1: // BNE
                taken = (reg[rs1] != reg[rs2]);
                break;
            case 0x4: // BLT
                taken = ((int32_t)reg[rs1] < (int32_t)reg[rs2]);
                break;
            case 0x5: // BGE
                taken = ((int32_t)reg[rs1] >= (int32_t)reg[rs2]);
                break;
            case 0x6: // BLTU
                taken = (reg[rs1] < reg[rs2]);
                break;
            case 0x7: // BGEU
                taken = (reg[rs1] >= reg[rs2]);
                break;
        }
        
        if (taken) {
            pc += imm;
        } else {
            pc += 4;
        }
    }
    
    void executeJAL() {
        reg[rd] = pc + 4;
        pc += imm;
    }
    
    void executeJALR() {
        uint32_t temp = pc + 4;
        pc = (reg[rs1] + imm) & ~1;
        reg[rd] = temp;
    }
    
public:
    RISCVSimulator() {
        memset(reg, 0, sizeof(reg));
        memset(memory, 0, sizeof(memory));
        pc = 0;
        running = true;
    }
    
    void loadProgram(const vector<uint32_t>& prog) {
        instructions = prog;
        // Load instructions into memory
        for (size_t i = 0; i < prog.size(); i++) {
            uint32_t inst = prog[i];
            memory[i*4] = inst & 0xFF;
            memory[i*4+1] = (inst >> 8) & 0xFF;
            memory[i*4+2] = (inst >> 16) & 0xFF;
            memory[i*4+3] = (inst >> 24) & 0xFF;
        }
    }
    
    void run() {
        while (running && pc/4 < instructions.size()) {
            uint32_t inst = instructions[pc/4];
            executeInstruction(inst);
        }
    }
    
    void printRegisters() {
        for (int i = 0; i < 32; i++) {
            cout << "0x" << hex << setw(8) << setfill('0') << reg[i];
            if (i < 31) cout << " ";
        }
        cout << endl;
    }
    
    uint32_t getRegister(int i) {
        return reg[i];
    }
};

int main() {
    vector<uint32_t> program;
    uint32_t inst;
    
    // Read instructions from stdin
    while (cin >> hex >> inst) {
        program.push_back(inst);
    }
    
    if (program.empty()) {
        return 0;
    }
    
    RISCVSimulator sim;
    sim.loadProgram(program);
    sim.run();
    sim.printRegisters();
    
    return 0;
}
