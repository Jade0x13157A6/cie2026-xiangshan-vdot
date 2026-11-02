module tb_vialufix_csa;

  logic  clock;
  logic  reset;
  logic  io_flush_valid;
  logic  io_flush_bits_isRVC;
  logic  io_flush_bits_robIdx_flag;
  logic [7:0] io_flush_bits_robIdx_value;
  logic  io_flush_bits_ftqIdx_flag;
  logic [5:0] io_flush_bits_ftqIdx_value;
  logic [3:0] io_flush_bits_ftqOffset;
  logic  io_flush_bits_level;
  logic  io_flush_bits_interrupt;
  logic [49:0] io_flush_bits_cfiUpdate_pc;
  logic  io_flush_bits_cfiUpdate_pd_valid;
  logic  io_flush_bits_cfiUpdate_pd_isRVC;
  logic [1:0] io_flush_bits_cfiUpdate_pd_brType;
  logic  io_flush_bits_cfiUpdate_pd_isCall;
  logic  io_flush_bits_cfiUpdate_pd_isRet;
  logic [3:0] io_flush_bits_cfiUpdate_ssp;
  logic [2:0] io_flush_bits_cfiUpdate_sctr;
  logic  io_flush_bits_cfiUpdate_TOSW_flag;
  logic [4:0] io_flush_bits_cfiUpdate_TOSW_value;
  logic  io_flush_bits_cfiUpdate_TOSR_flag;
  logic [4:0] io_flush_bits_cfiUpdate_TOSR_value;
  logic  io_flush_bits_cfiUpdate_NOS_flag;
  logic [4:0] io_flush_bits_cfiUpdate_NOS_value;
  logic [49:0] io_flush_bits_cfiUpdate_topAddr;
  logic [10:0] io_flush_bits_cfiUpdate_folded_hist_hist_17_folded_hist;
  logic [10:0] io_flush_bits_cfiUpdate_folded_hist_hist_16_folded_hist;
  logic [6:0] io_flush_bits_cfiUpdate_folded_hist_hist_15_folded_hist;
  logic [7:0] io_flush_bits_cfiUpdate_folded_hist_hist_14_folded_hist;
  logic [8:0] io_flush_bits_cfiUpdate_folded_hist_hist_13_folded_hist;
  logic [3:0] io_flush_bits_cfiUpdate_folded_hist_hist_12_folded_hist;
  logic [7:0] io_flush_bits_cfiUpdate_folded_hist_hist_11_folded_hist;
  logic [8:0] io_flush_bits_cfiUpdate_folded_hist_hist_10_folded_hist;
  logic [6:0] io_flush_bits_cfiUpdate_folded_hist_hist_9_folded_hist;
  logic [7:0] io_flush_bits_cfiUpdate_folded_hist_hist_8_folded_hist;
  logic [6:0] io_flush_bits_cfiUpdate_folded_hist_hist_7_folded_hist;
  logic [8:0] io_flush_bits_cfiUpdate_folded_hist_hist_6_folded_hist;
  logic [6:0] io_flush_bits_cfiUpdate_folded_hist_hist_5_folded_hist;
  logic [7:0] io_flush_bits_cfiUpdate_folded_hist_hist_4_folded_hist;
  logic [7:0] io_flush_bits_cfiUpdate_folded_hist_hist_3_folded_hist;
  logic [7:0] io_flush_bits_cfiUpdate_folded_hist_hist_2_folded_hist;
  logic [10:0] io_flush_bits_cfiUpdate_folded_hist_hist_1_folded_hist;
  logic [7:0] io_flush_bits_cfiUpdate_folded_hist_hist_0_folded_hist;
  logic  io_flush_bits_cfiUpdate_afhob_afhob_5_bits_0;
  logic  io_flush_bits_cfiUpdate_afhob_afhob_5_bits_1;
  logic  io_flush_bits_cfiUpdate_afhob_afhob_5_bits_2;
  logic  io_flush_bits_cfiUpdate_afhob_afhob_5_bits_3;
  logic  io_flush_bits_cfiUpdate_afhob_afhob_4_bits_0;
  logic  io_flush_bits_cfiUpdate_afhob_afhob_4_bits_1;
  logic  io_flush_bits_cfiUpdate_afhob_afhob_4_bits_2;
  logic  io_flush_bits_cfiUpdate_afhob_afhob_4_bits_3;
  logic  io_flush_bits_cfiUpdate_afhob_afhob_3_bits_0;
  logic  io_flush_bits_cfiUpdate_afhob_afhob_3_bits_1;
  logic  io_flush_bits_cfiUpdate_afhob_afhob_3_bits_2;
  logic  io_flush_bits_cfiUpdate_afhob_afhob_3_bits_3;
  logic  io_flush_bits_cfiUpdate_afhob_afhob_2_bits_0;
  logic  io_flush_bits_cfiUpdate_afhob_afhob_2_bits_1;
  logic  io_flush_bits_cfiUpdate_afhob_afhob_2_bits_2;
  logic  io_flush_bits_cfiUpdate_afhob_afhob_2_bits_3;
  logic  io_flush_bits_cfiUpdate_afhob_afhob_1_bits_0;
  logic  io_flush_bits_cfiUpdate_afhob_afhob_1_bits_1;
  logic  io_flush_bits_cfiUpdate_afhob_afhob_1_bits_2;
  logic  io_flush_bits_cfiUpdate_afhob_afhob_1_bits_3;
  logic  io_flush_bits_cfiUpdate_afhob_afhob_0_bits_0;
  logic  io_flush_bits_cfiUpdate_afhob_afhob_0_bits_1;
  logic  io_flush_bits_cfiUpdate_afhob_afhob_0_bits_2;
  logic  io_flush_bits_cfiUpdate_afhob_afhob_0_bits_3;
  logic [2:0] io_flush_bits_cfiUpdate_lastBrNumOH;
  logic [3:0] io_flush_bits_cfiUpdate_ghr;
  logic  io_flush_bits_cfiUpdate_histPtr_flag;
  logic [7:0] io_flush_bits_cfiUpdate_histPtr_value;
  logic [9:0] io_flush_bits_cfiUpdate_specCnt_0;
  logic [9:0] io_flush_bits_cfiUpdate_specCnt_1;
  logic  io_flush_bits_cfiUpdate_br_hit;
  logic  io_flush_bits_cfiUpdate_jr_hit;
  logic  io_flush_bits_cfiUpdate_sc_hit;
  logic  io_flush_bits_cfiUpdate_predTaken;
  logic [49:0] io_flush_bits_cfiUpdate_target;
  logic  io_flush_bits_cfiUpdate_taken;
  logic  io_flush_bits_cfiUpdate_isMisPred;
  logic [1:0] io_flush_bits_cfiUpdate_shift;
  logic  io_flush_bits_cfiUpdate_addIntoHist;
  logic  io_flush_bits_cfiUpdate_backendIGPF;
  logic  io_flush_bits_cfiUpdate_backendIPF;
  logic  io_flush_bits_cfiUpdate_backendIAF;
  logic [63:0] io_flush_bits_fullTarget;
  logic  io_flush_bits_stFtqIdx_flag;
  logic [5:0] io_flush_bits_stFtqIdx_value;
  logic [3:0] io_flush_bits_stFtqOffset;
  logic [63:0] io_flush_bits_debug_runahead_checkpoint_id;
  logic  io_flush_bits_debugIsCtrl;
  logic  io_flush_bits_debugIsMemVio;
  logic  io_in_ready;
  logic  io_in_valid;
  logic [8:0] io_in_bits_ctrl_fuOpType;
  logic  io_in_bits_ctrl_robIdx_flag;
  logic [7:0] io_in_bits_ctrl_robIdx_value;
  logic [7:0] io_in_bits_ctrl_pdest;
  logic  io_in_bits_ctrl_vecWen;
  logic  io_in_bits_ctrl_v0Wen;
  logic  io_in_bits_ctrl_vpu_vill;
  logic  io_in_bits_ctrl_vpu_vma;
  logic  io_in_bits_ctrl_vpu_vta;
  logic [1:0] io_in_bits_ctrl_vpu_vsew;
  logic [2:0] io_in_bits_ctrl_vpu_vlmul;
  logic  io_in_bits_ctrl_vpu_specVill;
  logic  io_in_bits_ctrl_vpu_specVma;
  logic  io_in_bits_ctrl_vpu_specVta;
  logic [1:0] io_in_bits_ctrl_vpu_specVsew;
  logic [2:0] io_in_bits_ctrl_vpu_specVlmul;
  logic  io_in_bits_ctrl_vpu_vm;
  logic [7:0] io_in_bits_ctrl_vpu_vstart;
  logic [2:0] io_in_bits_ctrl_vpu_frm;
  logic  io_in_bits_ctrl_vpu_fpu_isFpToVecInst;
  logic  io_in_bits_ctrl_vpu_fpu_isFP32Instr;
  logic  io_in_bits_ctrl_vpu_fpu_isFP64Instr;
  logic  io_in_bits_ctrl_vpu_fpu_isReduction;
  logic  io_in_bits_ctrl_vpu_fpu_isFoldTo1_2;
  logic  io_in_bits_ctrl_vpu_fpu_isFoldTo1_4;
  logic  io_in_bits_ctrl_vpu_fpu_isFoldTo1_8;
  logic [1:0] io_in_bits_ctrl_vpu_vxrm;
  logic [6:0] io_in_bits_ctrl_vpu_vuopIdx;
  logic  io_in_bits_ctrl_vpu_lastUop;
  logic [127:0] io_in_bits_ctrl_vpu_vmask;
  logic [7:0] io_in_bits_ctrl_vpu_vl;
  logic [2:0] io_in_bits_ctrl_vpu_nf;
  logic [1:0] io_in_bits_ctrl_vpu_veew;
  logic  io_in_bits_ctrl_vpu_isReverse;
  logic  io_in_bits_ctrl_vpu_isExt;
  logic  io_in_bits_ctrl_vpu_isNarrow;
  logic  io_in_bits_ctrl_vpu_isDstMask;
  logic  io_in_bits_ctrl_vpu_isOpMask;
  logic  io_in_bits_ctrl_vpu_isMove;
  logic  io_in_bits_ctrl_vpu_isDependOldVd;
  logic  io_in_bits_ctrl_vpu_isWritePartVd;
  logic  io_in_bits_ctrl_vpu_isVleff;
  logic [8:0] io_in_bits_ctrlPipe_0_fuOpType;
  logic  io_in_bits_ctrlPipe_0_robIdx_flag;
  logic [7:0] io_in_bits_ctrlPipe_0_robIdx_value;
  logic [7:0] io_in_bits_ctrlPipe_0_pdest;
  logic  io_in_bits_ctrlPipe_0_vecWen;
  logic  io_in_bits_ctrlPipe_0_v0Wen;
  logic  io_in_bits_ctrlPipe_0_vpu_vill;
  logic  io_in_bits_ctrlPipe_0_vpu_vma;
  logic  io_in_bits_ctrlPipe_0_vpu_vta;
  logic [1:0] io_in_bits_ctrlPipe_0_vpu_vsew;
  logic [2:0] io_in_bits_ctrlPipe_0_vpu_vlmul;
  logic  io_in_bits_ctrlPipe_0_vpu_specVill;
  logic  io_in_bits_ctrlPipe_0_vpu_specVma;
  logic  io_in_bits_ctrlPipe_0_vpu_specVta;
  logic [1:0] io_in_bits_ctrlPipe_0_vpu_specVsew;
  logic [2:0] io_in_bits_ctrlPipe_0_vpu_specVlmul;
  logic  io_in_bits_ctrlPipe_0_vpu_vm;
  logic [7:0] io_in_bits_ctrlPipe_0_vpu_vstart;
  logic [2:0] io_in_bits_ctrlPipe_0_vpu_frm;
  logic  io_in_bits_ctrlPipe_0_vpu_fpu_isFpToVecInst;
  logic  io_in_bits_ctrlPipe_0_vpu_fpu_isFP32Instr;
  logic  io_in_bits_ctrlPipe_0_vpu_fpu_isFP64Instr;
  logic  io_in_bits_ctrlPipe_0_vpu_fpu_isReduction;
  logic  io_in_bits_ctrlPipe_0_vpu_fpu_isFoldTo1_2;
  logic  io_in_bits_ctrlPipe_0_vpu_fpu_isFoldTo1_4;
  logic  io_in_bits_ctrlPipe_0_vpu_fpu_isFoldTo1_8;
  logic [1:0] io_in_bits_ctrlPipe_0_vpu_vxrm;
  logic [6:0] io_in_bits_ctrlPipe_0_vpu_vuopIdx;
  logic  io_in_bits_ctrlPipe_0_vpu_lastUop;
  logic [127:0] io_in_bits_ctrlPipe_0_vpu_vmask;
  logic [7:0] io_in_bits_ctrlPipe_0_vpu_vl;
  logic [2:0] io_in_bits_ctrlPipe_0_vpu_nf;
  logic [1:0] io_in_bits_ctrlPipe_0_vpu_veew;
  logic  io_in_bits_ctrlPipe_0_vpu_isReverse;
  logic  io_in_bits_ctrlPipe_0_vpu_isExt;
  logic  io_in_bits_ctrlPipe_0_vpu_isNarrow;
  logic  io_in_bits_ctrlPipe_0_vpu_isDstMask;
  logic  io_in_bits_ctrlPipe_0_vpu_isOpMask;
  logic  io_in_bits_ctrlPipe_0_vpu_isMove;
  logic  io_in_bits_ctrlPipe_0_vpu_isDependOldVd;
  logic  io_in_bits_ctrlPipe_0_vpu_isWritePartVd;
  logic  io_in_bits_ctrlPipe_0_vpu_isVleff;
  logic [8:0] io_in_bits_ctrlPipe_1_fuOpType;
  logic  io_in_bits_ctrlPipe_1_robIdx_flag;
  logic [7:0] io_in_bits_ctrlPipe_1_robIdx_value;
  logic [7:0] io_in_bits_ctrlPipe_1_pdest;
  logic  io_in_bits_ctrlPipe_1_vecWen;
  logic  io_in_bits_ctrlPipe_1_v0Wen;
  logic  io_in_bits_ctrlPipe_1_vpu_vill;
  logic  io_in_bits_ctrlPipe_1_vpu_vma;
  logic  io_in_bits_ctrlPipe_1_vpu_vta;
  logic [1:0] io_in_bits_ctrlPipe_1_vpu_vsew;
  logic [2:0] io_in_bits_ctrlPipe_1_vpu_vlmul;
  logic  io_in_bits_ctrlPipe_1_vpu_specVill;
  logic  io_in_bits_ctrlPipe_1_vpu_specVma;
  logic  io_in_bits_ctrlPipe_1_vpu_specVta;
  logic [1:0] io_in_bits_ctrlPipe_1_vpu_specVsew;
  logic [2:0] io_in_bits_ctrlPipe_1_vpu_specVlmul;
  logic  io_in_bits_ctrlPipe_1_vpu_vm;
  logic [7:0] io_in_bits_ctrlPipe_1_vpu_vstart;
  logic [2:0] io_in_bits_ctrlPipe_1_vpu_frm;
  logic  io_in_bits_ctrlPipe_1_vpu_fpu_isFpToVecInst;
  logic  io_in_bits_ctrlPipe_1_vpu_fpu_isFP32Instr;
  logic  io_in_bits_ctrlPipe_1_vpu_fpu_isFP64Instr;
  logic  io_in_bits_ctrlPipe_1_vpu_fpu_isReduction;
  logic  io_in_bits_ctrlPipe_1_vpu_fpu_isFoldTo1_2;
  logic  io_in_bits_ctrlPipe_1_vpu_fpu_isFoldTo1_4;
  logic  io_in_bits_ctrlPipe_1_vpu_fpu_isFoldTo1_8;
  logic [1:0] io_in_bits_ctrlPipe_1_vpu_vxrm;
  logic [6:0] io_in_bits_ctrlPipe_1_vpu_vuopIdx;
  logic  io_in_bits_ctrlPipe_1_vpu_lastUop;
  logic [127:0] io_in_bits_ctrlPipe_1_vpu_vmask;
  logic [7:0] io_in_bits_ctrlPipe_1_vpu_vl;
  logic [2:0] io_in_bits_ctrlPipe_1_vpu_nf;
  logic [1:0] io_in_bits_ctrlPipe_1_vpu_veew;
  logic  io_in_bits_ctrlPipe_1_vpu_isReverse;
  logic  io_in_bits_ctrlPipe_1_vpu_isExt;
  logic  io_in_bits_ctrlPipe_1_vpu_isNarrow;
  logic  io_in_bits_ctrlPipe_1_vpu_isDstMask;
  logic  io_in_bits_ctrlPipe_1_vpu_isOpMask;
  logic  io_in_bits_ctrlPipe_1_vpu_isMove;
  logic  io_in_bits_ctrlPipe_1_vpu_isDependOldVd;
  logic  io_in_bits_ctrlPipe_1_vpu_isWritePartVd;
  logic  io_in_bits_ctrlPipe_1_vpu_isVleff;
  logic  io_in_bits_validPipe_0;
  logic  io_in_bits_validPipe_1;
  logic [7:0] io_in_bits_data_src_4;
  logic [127:0] io_in_bits_data_src_3;
  logic [127:0] io_in_bits_data_src_2;
  logic [127:0] io_in_bits_data_src_1;
  logic [127:0] io_in_bits_data_src_0;
  logic [127:0] io_in_bits_data_imm;
  logic [7:0] io_in_bits_dataPipe_0_src_4;
  logic [127:0] io_in_bits_dataPipe_0_src_3;
  logic [127:0] io_in_bits_dataPipe_0_src_2;
  logic [127:0] io_in_bits_dataPipe_0_src_1;
  logic [127:0] io_in_bits_dataPipe_0_src_0;
  logic [127:0] io_in_bits_dataPipe_0_imm;
  logic [7:0] io_in_bits_dataPipe_1_src_4;
  logic [127:0] io_in_bits_dataPipe_1_src_3;
  logic [127:0] io_in_bits_dataPipe_1_src_2;
  logic [127:0] io_in_bits_dataPipe_1_src_1;
  logic [127:0] io_in_bits_dataPipe_1_src_0;
  logic [127:0] io_in_bits_dataPipe_1_imm;
  logic  io_in_bits_perfDebugInfo_eliminatedMove;
  logic [63:0] io_in_bits_perfDebugInfo_renameTime;
  logic [63:0] io_in_bits_perfDebugInfo_dispatchTime;
  logic [63:0] io_in_bits_perfDebugInfo_enqRsTime;
  logic [63:0] io_in_bits_perfDebugInfo_selectTime;
  logic [63:0] io_in_bits_perfDebugInfo_issueTime;
  logic [63:0] io_in_bits_perfDebugInfo_writebackTime;
  logic [63:0] io_in_bits_perfDebugInfo_runahead_checkpoint_id;
  logic [63:0] io_in_bits_perfDebugInfo_tlbFirstReqTime;
  logic [63:0] io_in_bits_perfDebugInfo_tlbRespTime;
  logic  io_out_ready;
  logic  io_out_valid;
  logic [127:0] io_out_bits_res_data;
  logic [1:0] io_vxrm;

  VIAluFix dut (
    .clock(clock),
    .reset(reset),
    .io_flush_valid(io_flush_valid),
    .io_flush_bits_isRVC(io_flush_bits_isRVC),
    .io_flush_bits_robIdx_flag(io_flush_bits_robIdx_flag),
    .io_flush_bits_robIdx_value(io_flush_bits_robIdx_value),
    .io_flush_bits_ftqIdx_flag(io_flush_bits_ftqIdx_flag),
    .io_flush_bits_ftqIdx_value(io_flush_bits_ftqIdx_value),
    .io_flush_bits_ftqOffset(io_flush_bits_ftqOffset),
    .io_flush_bits_level(io_flush_bits_level),
    .io_flush_bits_interrupt(io_flush_bits_interrupt),
    .io_flush_bits_cfiUpdate_pc(io_flush_bits_cfiUpdate_pc),
    .io_flush_bits_cfiUpdate_pd_valid(io_flush_bits_cfiUpdate_pd_valid),
    .io_flush_bits_cfiUpdate_pd_isRVC(io_flush_bits_cfiUpdate_pd_isRVC),
    .io_flush_bits_cfiUpdate_pd_brType(io_flush_bits_cfiUpdate_pd_brType),
    .io_flush_bits_cfiUpdate_pd_isCall(io_flush_bits_cfiUpdate_pd_isCall),
    .io_flush_bits_cfiUpdate_pd_isRet(io_flush_bits_cfiUpdate_pd_isRet),
    .io_flush_bits_cfiUpdate_ssp(io_flush_bits_cfiUpdate_ssp),
    .io_flush_bits_cfiUpdate_sctr(io_flush_bits_cfiUpdate_sctr),
    .io_flush_bits_cfiUpdate_TOSW_flag(io_flush_bits_cfiUpdate_TOSW_flag),
    .io_flush_bits_cfiUpdate_TOSW_value(io_flush_bits_cfiUpdate_TOSW_value),
    .io_flush_bits_cfiUpdate_TOSR_flag(io_flush_bits_cfiUpdate_TOSR_flag),
    .io_flush_bits_cfiUpdate_TOSR_value(io_flush_bits_cfiUpdate_TOSR_value),
    .io_flush_bits_cfiUpdate_NOS_flag(io_flush_bits_cfiUpdate_NOS_flag),
    .io_flush_bits_cfiUpdate_NOS_value(io_flush_bits_cfiUpdate_NOS_value),
    .io_flush_bits_cfiUpdate_topAddr(io_flush_bits_cfiUpdate_topAddr),
    .io_flush_bits_cfiUpdate_folded_hist_hist_17_folded_hist(io_flush_bits_cfiUpdate_folded_hist_hist_17_folded_hist),
    .io_flush_bits_cfiUpdate_folded_hist_hist_16_folded_hist(io_flush_bits_cfiUpdate_folded_hist_hist_16_folded_hist),
    .io_flush_bits_cfiUpdate_folded_hist_hist_15_folded_hist(io_flush_bits_cfiUpdate_folded_hist_hist_15_folded_hist),
    .io_flush_bits_cfiUpdate_folded_hist_hist_14_folded_hist(io_flush_bits_cfiUpdate_folded_hist_hist_14_folded_hist),
    .io_flush_bits_cfiUpdate_folded_hist_hist_13_folded_hist(io_flush_bits_cfiUpdate_folded_hist_hist_13_folded_hist),
    .io_flush_bits_cfiUpdate_folded_hist_hist_12_folded_hist(io_flush_bits_cfiUpdate_folded_hist_hist_12_folded_hist),
    .io_flush_bits_cfiUpdate_folded_hist_hist_11_folded_hist(io_flush_bits_cfiUpdate_folded_hist_hist_11_folded_hist),
    .io_flush_bits_cfiUpdate_folded_hist_hist_10_folded_hist(io_flush_bits_cfiUpdate_folded_hist_hist_10_folded_hist),
    .io_flush_bits_cfiUpdate_folded_hist_hist_9_folded_hist(io_flush_bits_cfiUpdate_folded_hist_hist_9_folded_hist),
    .io_flush_bits_cfiUpdate_folded_hist_hist_8_folded_hist(io_flush_bits_cfiUpdate_folded_hist_hist_8_folded_hist),
    .io_flush_bits_cfiUpdate_folded_hist_hist_7_folded_hist(io_flush_bits_cfiUpdate_folded_hist_hist_7_folded_hist),
    .io_flush_bits_cfiUpdate_folded_hist_hist_6_folded_hist(io_flush_bits_cfiUpdate_folded_hist_hist_6_folded_hist),
    .io_flush_bits_cfiUpdate_folded_hist_hist_5_folded_hist(io_flush_bits_cfiUpdate_folded_hist_hist_5_folded_hist),
    .io_flush_bits_cfiUpdate_folded_hist_hist_4_folded_hist(io_flush_bits_cfiUpdate_folded_hist_hist_4_folded_hist),
    .io_flush_bits_cfiUpdate_folded_hist_hist_3_folded_hist(io_flush_bits_cfiUpdate_folded_hist_hist_3_folded_hist),
    .io_flush_bits_cfiUpdate_folded_hist_hist_2_folded_hist(io_flush_bits_cfiUpdate_folded_hist_hist_2_folded_hist),
    .io_flush_bits_cfiUpdate_folded_hist_hist_1_folded_hist(io_flush_bits_cfiUpdate_folded_hist_hist_1_folded_hist),
    .io_flush_bits_cfiUpdate_folded_hist_hist_0_folded_hist(io_flush_bits_cfiUpdate_folded_hist_hist_0_folded_hist),
    .io_flush_bits_cfiUpdate_afhob_afhob_5_bits_0(io_flush_bits_cfiUpdate_afhob_afhob_5_bits_0),
    .io_flush_bits_cfiUpdate_afhob_afhob_5_bits_1(io_flush_bits_cfiUpdate_afhob_afhob_5_bits_1),
    .io_flush_bits_cfiUpdate_afhob_afhob_5_bits_2(io_flush_bits_cfiUpdate_afhob_afhob_5_bits_2),
    .io_flush_bits_cfiUpdate_afhob_afhob_5_bits_3(io_flush_bits_cfiUpdate_afhob_afhob_5_bits_3),
    .io_flush_bits_cfiUpdate_afhob_afhob_4_bits_0(io_flush_bits_cfiUpdate_afhob_afhob_4_bits_0),
    .io_flush_bits_cfiUpdate_afhob_afhob_4_bits_1(io_flush_bits_cfiUpdate_afhob_afhob_4_bits_1),
    .io_flush_bits_cfiUpdate_afhob_afhob_4_bits_2(io_flush_bits_cfiUpdate_afhob_afhob_4_bits_2),
    .io_flush_bits_cfiUpdate_afhob_afhob_4_bits_3(io_flush_bits_cfiUpdate_afhob_afhob_4_bits_3),
    .io_flush_bits_cfiUpdate_afhob_afhob_3_bits_0(io_flush_bits_cfiUpdate_afhob_afhob_3_bits_0),
    .io_flush_bits_cfiUpdate_afhob_afhob_3_bits_1(io_flush_bits_cfiUpdate_afhob_afhob_3_bits_1),
    .io_flush_bits_cfiUpdate_afhob_afhob_3_bits_2(io_flush_bits_cfiUpdate_afhob_afhob_3_bits_2),
    .io_flush_bits_cfiUpdate_afhob_afhob_3_bits_3(io_flush_bits_cfiUpdate_afhob_afhob_3_bits_3),
    .io_flush_bits_cfiUpdate_afhob_afhob_2_bits_0(io_flush_bits_cfiUpdate_afhob_afhob_2_bits_0),
    .io_flush_bits_cfiUpdate_afhob_afhob_2_bits_1(io_flush_bits_cfiUpdate_afhob_afhob_2_bits_1),
    .io_flush_bits_cfiUpdate_afhob_afhob_2_bits_2(io_flush_bits_cfiUpdate_afhob_afhob_2_bits_2),
    .io_flush_bits_cfiUpdate_afhob_afhob_2_bits_3(io_flush_bits_cfiUpdate_afhob_afhob_2_bits_3),
    .io_flush_bits_cfiUpdate_afhob_afhob_1_bits_0(io_flush_bits_cfiUpdate_afhob_afhob_1_bits_0),
    .io_flush_bits_cfiUpdate_afhob_afhob_1_bits_1(io_flush_bits_cfiUpdate_afhob_afhob_1_bits_1),
    .io_flush_bits_cfiUpdate_afhob_afhob_1_bits_2(io_flush_bits_cfiUpdate_afhob_afhob_1_bits_2),
    .io_flush_bits_cfiUpdate_afhob_afhob_1_bits_3(io_flush_bits_cfiUpdate_afhob_afhob_1_bits_3),
    .io_flush_bits_cfiUpdate_afhob_afhob_0_bits_0(io_flush_bits_cfiUpdate_afhob_afhob_0_bits_0),
    .io_flush_bits_cfiUpdate_afhob_afhob_0_bits_1(io_flush_bits_cfiUpdate_afhob_afhob_0_bits_1),
    .io_flush_bits_cfiUpdate_afhob_afhob_0_bits_2(io_flush_bits_cfiUpdate_afhob_afhob_0_bits_2),
    .io_flush_bits_cfiUpdate_afhob_afhob_0_bits_3(io_flush_bits_cfiUpdate_afhob_afhob_0_bits_3),
    .io_flush_bits_cfiUpdate_lastBrNumOH(io_flush_bits_cfiUpdate_lastBrNumOH),
    .io_flush_bits_cfiUpdate_ghr(io_flush_bits_cfiUpdate_ghr),
    .io_flush_bits_cfiUpdate_histPtr_flag(io_flush_bits_cfiUpdate_histPtr_flag),
    .io_flush_bits_cfiUpdate_histPtr_value(io_flush_bits_cfiUpdate_histPtr_value),
    .io_flush_bits_cfiUpdate_specCnt_0(io_flush_bits_cfiUpdate_specCnt_0),
    .io_flush_bits_cfiUpdate_specCnt_1(io_flush_bits_cfiUpdate_specCnt_1),
    .io_flush_bits_cfiUpdate_br_hit(io_flush_bits_cfiUpdate_br_hit),
    .io_flush_bits_cfiUpdate_jr_hit(io_flush_bits_cfiUpdate_jr_hit),
    .io_flush_bits_cfiUpdate_sc_hit(io_flush_bits_cfiUpdate_sc_hit),
    .io_flush_bits_cfiUpdate_predTaken(io_flush_bits_cfiUpdate_predTaken),
    .io_flush_bits_cfiUpdate_target(io_flush_bits_cfiUpdate_target),
    .io_flush_bits_cfiUpdate_taken(io_flush_bits_cfiUpdate_taken),
    .io_flush_bits_cfiUpdate_isMisPred(io_flush_bits_cfiUpdate_isMisPred),
    .io_flush_bits_cfiUpdate_shift(io_flush_bits_cfiUpdate_shift),
    .io_flush_bits_cfiUpdate_addIntoHist(io_flush_bits_cfiUpdate_addIntoHist),
    .io_flush_bits_cfiUpdate_backendIGPF(io_flush_bits_cfiUpdate_backendIGPF),
    .io_flush_bits_cfiUpdate_backendIPF(io_flush_bits_cfiUpdate_backendIPF),
    .io_flush_bits_cfiUpdate_backendIAF(io_flush_bits_cfiUpdate_backendIAF),
    .io_flush_bits_fullTarget(io_flush_bits_fullTarget),
    .io_flush_bits_stFtqIdx_flag(io_flush_bits_stFtqIdx_flag),
    .io_flush_bits_stFtqIdx_value(io_flush_bits_stFtqIdx_value),
    .io_flush_bits_stFtqOffset(io_flush_bits_stFtqOffset),
    .io_flush_bits_debug_runahead_checkpoint_id(io_flush_bits_debug_runahead_checkpoint_id),
    .io_flush_bits_debugIsCtrl(io_flush_bits_debugIsCtrl),
    .io_flush_bits_debugIsMemVio(io_flush_bits_debugIsMemVio),
    .io_in_ready(io_in_ready),
    .io_in_valid(io_in_valid),
    .io_in_bits_ctrl_fuOpType(io_in_bits_ctrl_fuOpType),
    .io_in_bits_ctrl_robIdx_flag(io_in_bits_ctrl_robIdx_flag),
    .io_in_bits_ctrl_robIdx_value(io_in_bits_ctrl_robIdx_value),
    .io_in_bits_ctrl_pdest(io_in_bits_ctrl_pdest),
    .io_in_bits_ctrl_vecWen(io_in_bits_ctrl_vecWen),
    .io_in_bits_ctrl_v0Wen(io_in_bits_ctrl_v0Wen),
    .io_in_bits_ctrl_vpu_vill(io_in_bits_ctrl_vpu_vill),
    .io_in_bits_ctrl_vpu_vma(io_in_bits_ctrl_vpu_vma),
    .io_in_bits_ctrl_vpu_vta(io_in_bits_ctrl_vpu_vta),
    .io_in_bits_ctrl_vpu_vsew(io_in_bits_ctrl_vpu_vsew),
    .io_in_bits_ctrl_vpu_vlmul(io_in_bits_ctrl_vpu_vlmul),
    .io_in_bits_ctrl_vpu_specVill(io_in_bits_ctrl_vpu_specVill),
    .io_in_bits_ctrl_vpu_specVma(io_in_bits_ctrl_vpu_specVma),
    .io_in_bits_ctrl_vpu_specVta(io_in_bits_ctrl_vpu_specVta),
    .io_in_bits_ctrl_vpu_specVsew(io_in_bits_ctrl_vpu_specVsew),
    .io_in_bits_ctrl_vpu_specVlmul(io_in_bits_ctrl_vpu_specVlmul),
    .io_in_bits_ctrl_vpu_vm(io_in_bits_ctrl_vpu_vm),
    .io_in_bits_ctrl_vpu_vstart(io_in_bits_ctrl_vpu_vstart),
    .io_in_bits_ctrl_vpu_frm(io_in_bits_ctrl_vpu_frm),
    .io_in_bits_ctrl_vpu_fpu_isFpToVecInst(io_in_bits_ctrl_vpu_fpu_isFpToVecInst),
    .io_in_bits_ctrl_vpu_fpu_isFP32Instr(io_in_bits_ctrl_vpu_fpu_isFP32Instr),
    .io_in_bits_ctrl_vpu_fpu_isFP64Instr(io_in_bits_ctrl_vpu_fpu_isFP64Instr),
    .io_in_bits_ctrl_vpu_fpu_isReduction(io_in_bits_ctrl_vpu_fpu_isReduction),
    .io_in_bits_ctrl_vpu_fpu_isFoldTo1_2(io_in_bits_ctrl_vpu_fpu_isFoldTo1_2),
    .io_in_bits_ctrl_vpu_fpu_isFoldTo1_4(io_in_bits_ctrl_vpu_fpu_isFoldTo1_4),
    .io_in_bits_ctrl_vpu_fpu_isFoldTo1_8(io_in_bits_ctrl_vpu_fpu_isFoldTo1_8),
    .io_in_bits_ctrl_vpu_vxrm(io_in_bits_ctrl_vpu_vxrm),
    .io_in_bits_ctrl_vpu_vuopIdx(io_in_bits_ctrl_vpu_vuopIdx),
    .io_in_bits_ctrl_vpu_lastUop(io_in_bits_ctrl_vpu_lastUop),
    .io_in_bits_ctrl_vpu_vmask(io_in_bits_ctrl_vpu_vmask),
    .io_in_bits_ctrl_vpu_vl(io_in_bits_ctrl_vpu_vl),
    .io_in_bits_ctrl_vpu_nf(io_in_bits_ctrl_vpu_nf),
    .io_in_bits_ctrl_vpu_veew(io_in_bits_ctrl_vpu_veew),
    .io_in_bits_ctrl_vpu_isReverse(io_in_bits_ctrl_vpu_isReverse),
    .io_in_bits_ctrl_vpu_isExt(io_in_bits_ctrl_vpu_isExt),
    .io_in_bits_ctrl_vpu_isNarrow(io_in_bits_ctrl_vpu_isNarrow),
    .io_in_bits_ctrl_vpu_isDstMask(io_in_bits_ctrl_vpu_isDstMask),
    .io_in_bits_ctrl_vpu_isOpMask(io_in_bits_ctrl_vpu_isOpMask),
    .io_in_bits_ctrl_vpu_isMove(io_in_bits_ctrl_vpu_isMove),
    .io_in_bits_ctrl_vpu_isDependOldVd(io_in_bits_ctrl_vpu_isDependOldVd),
    .io_in_bits_ctrl_vpu_isWritePartVd(io_in_bits_ctrl_vpu_isWritePartVd),
    .io_in_bits_ctrl_vpu_isVleff(io_in_bits_ctrl_vpu_isVleff),
    .io_in_bits_ctrlPipe_0_fuOpType(io_in_bits_ctrlPipe_0_fuOpType),
    .io_in_bits_ctrlPipe_0_robIdx_flag(io_in_bits_ctrlPipe_0_robIdx_flag),
    .io_in_bits_ctrlPipe_0_robIdx_value(io_in_bits_ctrlPipe_0_robIdx_value),
    .io_in_bits_ctrlPipe_0_pdest(io_in_bits_ctrlPipe_0_pdest),
    .io_in_bits_ctrlPipe_0_vecWen(io_in_bits_ctrlPipe_0_vecWen),
    .io_in_bits_ctrlPipe_0_v0Wen(io_in_bits_ctrlPipe_0_v0Wen),
    .io_in_bits_ctrlPipe_0_vpu_vill(io_in_bits_ctrlPipe_0_vpu_vill),
    .io_in_bits_ctrlPipe_0_vpu_vma(io_in_bits_ctrlPipe_0_vpu_vma),
    .io_in_bits_ctrlPipe_0_vpu_vta(io_in_bits_ctrlPipe_0_vpu_vta),
    .io_in_bits_ctrlPipe_0_vpu_vsew(io_in_bits_ctrlPipe_0_vpu_vsew),
    .io_in_bits_ctrlPipe_0_vpu_vlmul(io_in_bits_ctrlPipe_0_vpu_vlmul),
    .io_in_bits_ctrlPipe_0_vpu_specVill(io_in_bits_ctrlPipe_0_vpu_specVill),
    .io_in_bits_ctrlPipe_0_vpu_specVma(io_in_bits_ctrlPipe_0_vpu_specVma),
    .io_in_bits_ctrlPipe_0_vpu_specVta(io_in_bits_ctrlPipe_0_vpu_specVta),
    .io_in_bits_ctrlPipe_0_vpu_specVsew(io_in_bits_ctrlPipe_0_vpu_specVsew),
    .io_in_bits_ctrlPipe_0_vpu_specVlmul(io_in_bits_ctrlPipe_0_vpu_specVlmul),
    .io_in_bits_ctrlPipe_0_vpu_vm(io_in_bits_ctrlPipe_0_vpu_vm),
    .io_in_bits_ctrlPipe_0_vpu_vstart(io_in_bits_ctrlPipe_0_vpu_vstart),
    .io_in_bits_ctrlPipe_0_vpu_frm(io_in_bits_ctrlPipe_0_vpu_frm),
    .io_in_bits_ctrlPipe_0_vpu_fpu_isFpToVecInst(io_in_bits_ctrlPipe_0_vpu_fpu_isFpToVecInst),
    .io_in_bits_ctrlPipe_0_vpu_fpu_isFP32Instr(io_in_bits_ctrlPipe_0_vpu_fpu_isFP32Instr),
    .io_in_bits_ctrlPipe_0_vpu_fpu_isFP64Instr(io_in_bits_ctrlPipe_0_vpu_fpu_isFP64Instr),
    .io_in_bits_ctrlPipe_0_vpu_fpu_isReduction(io_in_bits_ctrlPipe_0_vpu_fpu_isReduction),
    .io_in_bits_ctrlPipe_0_vpu_fpu_isFoldTo1_2(io_in_bits_ctrlPipe_0_vpu_fpu_isFoldTo1_2),
    .io_in_bits_ctrlPipe_0_vpu_fpu_isFoldTo1_4(io_in_bits_ctrlPipe_0_vpu_fpu_isFoldTo1_4),
    .io_in_bits_ctrlPipe_0_vpu_fpu_isFoldTo1_8(io_in_bits_ctrlPipe_0_vpu_fpu_isFoldTo1_8),
    .io_in_bits_ctrlPipe_0_vpu_vxrm(io_in_bits_ctrlPipe_0_vpu_vxrm),
    .io_in_bits_ctrlPipe_0_vpu_vuopIdx(io_in_bits_ctrlPipe_0_vpu_vuopIdx),
    .io_in_bits_ctrlPipe_0_vpu_lastUop(io_in_bits_ctrlPipe_0_vpu_lastUop),
    .io_in_bits_ctrlPipe_0_vpu_vmask(io_in_bits_ctrlPipe_0_vpu_vmask),
    .io_in_bits_ctrlPipe_0_vpu_vl(io_in_bits_ctrlPipe_0_vpu_vl),
    .io_in_bits_ctrlPipe_0_vpu_nf(io_in_bits_ctrlPipe_0_vpu_nf),
    .io_in_bits_ctrlPipe_0_vpu_veew(io_in_bits_ctrlPipe_0_vpu_veew),
    .io_in_bits_ctrlPipe_0_vpu_isReverse(io_in_bits_ctrlPipe_0_vpu_isReverse),
    .io_in_bits_ctrlPipe_0_vpu_isExt(io_in_bits_ctrlPipe_0_vpu_isExt),
    .io_in_bits_ctrlPipe_0_vpu_isNarrow(io_in_bits_ctrlPipe_0_vpu_isNarrow),
    .io_in_bits_ctrlPipe_0_vpu_isDstMask(io_in_bits_ctrlPipe_0_vpu_isDstMask),
    .io_in_bits_ctrlPipe_0_vpu_isOpMask(io_in_bits_ctrlPipe_0_vpu_isOpMask),
    .io_in_bits_ctrlPipe_0_vpu_isMove(io_in_bits_ctrlPipe_0_vpu_isMove),
    .io_in_bits_ctrlPipe_0_vpu_isDependOldVd(io_in_bits_ctrlPipe_0_vpu_isDependOldVd),
    .io_in_bits_ctrlPipe_0_vpu_isWritePartVd(io_in_bits_ctrlPipe_0_vpu_isWritePartVd),
    .io_in_bits_ctrlPipe_0_vpu_isVleff(io_in_bits_ctrlPipe_0_vpu_isVleff),
    .io_in_bits_ctrlPipe_1_fuOpType(io_in_bits_ctrlPipe_1_fuOpType),
    .io_in_bits_ctrlPipe_1_robIdx_flag(io_in_bits_ctrlPipe_1_robIdx_flag),
    .io_in_bits_ctrlPipe_1_robIdx_value(io_in_bits_ctrlPipe_1_robIdx_value),
    .io_in_bits_ctrlPipe_1_pdest(io_in_bits_ctrlPipe_1_pdest),
    .io_in_bits_ctrlPipe_1_vecWen(io_in_bits_ctrlPipe_1_vecWen),
    .io_in_bits_ctrlPipe_1_v0Wen(io_in_bits_ctrlPipe_1_v0Wen),
    .io_in_bits_ctrlPipe_1_vpu_vill(io_in_bits_ctrlPipe_1_vpu_vill),
    .io_in_bits_ctrlPipe_1_vpu_vma(io_in_bits_ctrlPipe_1_vpu_vma),
    .io_in_bits_ctrlPipe_1_vpu_vta(io_in_bits_ctrlPipe_1_vpu_vta),
    .io_in_bits_ctrlPipe_1_vpu_vsew(io_in_bits_ctrlPipe_1_vpu_vsew),
    .io_in_bits_ctrlPipe_1_vpu_vlmul(io_in_bits_ctrlPipe_1_vpu_vlmul),
    .io_in_bits_ctrlPipe_1_vpu_specVill(io_in_bits_ctrlPipe_1_vpu_specVill),
    .io_in_bits_ctrlPipe_1_vpu_specVma(io_in_bits_ctrlPipe_1_vpu_specVma),
    .io_in_bits_ctrlPipe_1_vpu_specVta(io_in_bits_ctrlPipe_1_vpu_specVta),
    .io_in_bits_ctrlPipe_1_vpu_specVsew(io_in_bits_ctrlPipe_1_vpu_specVsew),
    .io_in_bits_ctrlPipe_1_vpu_specVlmul(io_in_bits_ctrlPipe_1_vpu_specVlmul),
    .io_in_bits_ctrlPipe_1_vpu_vm(io_in_bits_ctrlPipe_1_vpu_vm),
    .io_in_bits_ctrlPipe_1_vpu_vstart(io_in_bits_ctrlPipe_1_vpu_vstart),
    .io_in_bits_ctrlPipe_1_vpu_frm(io_in_bits_ctrlPipe_1_vpu_frm),
    .io_in_bits_ctrlPipe_1_vpu_fpu_isFpToVecInst(io_in_bits_ctrlPipe_1_vpu_fpu_isFpToVecInst),
    .io_in_bits_ctrlPipe_1_vpu_fpu_isFP32Instr(io_in_bits_ctrlPipe_1_vpu_fpu_isFP32Instr),
    .io_in_bits_ctrlPipe_1_vpu_fpu_isFP64Instr(io_in_bits_ctrlPipe_1_vpu_fpu_isFP64Instr),
    .io_in_bits_ctrlPipe_1_vpu_fpu_isReduction(io_in_bits_ctrlPipe_1_vpu_fpu_isReduction),
    .io_in_bits_ctrlPipe_1_vpu_fpu_isFoldTo1_2(io_in_bits_ctrlPipe_1_vpu_fpu_isFoldTo1_2),
    .io_in_bits_ctrlPipe_1_vpu_fpu_isFoldTo1_4(io_in_bits_ctrlPipe_1_vpu_fpu_isFoldTo1_4),
    .io_in_bits_ctrlPipe_1_vpu_fpu_isFoldTo1_8(io_in_bits_ctrlPipe_1_vpu_fpu_isFoldTo1_8),
    .io_in_bits_ctrlPipe_1_vpu_vxrm(io_in_bits_ctrlPipe_1_vpu_vxrm),
    .io_in_bits_ctrlPipe_1_vpu_vuopIdx(io_in_bits_ctrlPipe_1_vpu_vuopIdx),
    .io_in_bits_ctrlPipe_1_vpu_lastUop(io_in_bits_ctrlPipe_1_vpu_lastUop),
    .io_in_bits_ctrlPipe_1_vpu_vmask(io_in_bits_ctrlPipe_1_vpu_vmask),
    .io_in_bits_ctrlPipe_1_vpu_vl(io_in_bits_ctrlPipe_1_vpu_vl),
    .io_in_bits_ctrlPipe_1_vpu_nf(io_in_bits_ctrlPipe_1_vpu_nf),
    .io_in_bits_ctrlPipe_1_vpu_veew(io_in_bits_ctrlPipe_1_vpu_veew),
    .io_in_bits_ctrlPipe_1_vpu_isReverse(io_in_bits_ctrlPipe_1_vpu_isReverse),
    .io_in_bits_ctrlPipe_1_vpu_isExt(io_in_bits_ctrlPipe_1_vpu_isExt),
    .io_in_bits_ctrlPipe_1_vpu_isNarrow(io_in_bits_ctrlPipe_1_vpu_isNarrow),
    .io_in_bits_ctrlPipe_1_vpu_isDstMask(io_in_bits_ctrlPipe_1_vpu_isDstMask),
    .io_in_bits_ctrlPipe_1_vpu_isOpMask(io_in_bits_ctrlPipe_1_vpu_isOpMask),
    .io_in_bits_ctrlPipe_1_vpu_isMove(io_in_bits_ctrlPipe_1_vpu_isMove),
    .io_in_bits_ctrlPipe_1_vpu_isDependOldVd(io_in_bits_ctrlPipe_1_vpu_isDependOldVd),
    .io_in_bits_ctrlPipe_1_vpu_isWritePartVd(io_in_bits_ctrlPipe_1_vpu_isWritePartVd),
    .io_in_bits_ctrlPipe_1_vpu_isVleff(io_in_bits_ctrlPipe_1_vpu_isVleff),
    .io_in_bits_validPipe_0(io_in_bits_validPipe_0),
    .io_in_bits_validPipe_1(io_in_bits_validPipe_1),
    .io_in_bits_data_src_4(io_in_bits_data_src_4),
    .io_in_bits_data_src_3(io_in_bits_data_src_3),
    .io_in_bits_data_src_2(io_in_bits_data_src_2),
    .io_in_bits_data_src_1(io_in_bits_data_src_1),
    .io_in_bits_data_src_0(io_in_bits_data_src_0),
    .io_in_bits_data_imm(io_in_bits_data_imm),
    .io_in_bits_dataPipe_0_src_4(io_in_bits_dataPipe_0_src_4),
    .io_in_bits_dataPipe_0_src_3(io_in_bits_dataPipe_0_src_3),
    .io_in_bits_dataPipe_0_src_2(io_in_bits_dataPipe_0_src_2),
    .io_in_bits_dataPipe_0_src_1(io_in_bits_dataPipe_0_src_1),
    .io_in_bits_dataPipe_0_src_0(io_in_bits_dataPipe_0_src_0),
    .io_in_bits_dataPipe_0_imm(io_in_bits_dataPipe_0_imm),
    .io_in_bits_dataPipe_1_src_4(io_in_bits_dataPipe_1_src_4),
    .io_in_bits_dataPipe_1_src_3(io_in_bits_dataPipe_1_src_3),
    .io_in_bits_dataPipe_1_src_2(io_in_bits_dataPipe_1_src_2),
    .io_in_bits_dataPipe_1_src_1(io_in_bits_dataPipe_1_src_1),
    .io_in_bits_dataPipe_1_src_0(io_in_bits_dataPipe_1_src_0),
    .io_in_bits_dataPipe_1_imm(io_in_bits_dataPipe_1_imm),
    .io_in_bits_perfDebugInfo_eliminatedMove(io_in_bits_perfDebugInfo_eliminatedMove),
    .io_in_bits_perfDebugInfo_renameTime(io_in_bits_perfDebugInfo_renameTime),
    .io_in_bits_perfDebugInfo_dispatchTime(io_in_bits_perfDebugInfo_dispatchTime),
    .io_in_bits_perfDebugInfo_enqRsTime(io_in_bits_perfDebugInfo_enqRsTime),
    .io_in_bits_perfDebugInfo_selectTime(io_in_bits_perfDebugInfo_selectTime),
    .io_in_bits_perfDebugInfo_issueTime(io_in_bits_perfDebugInfo_issueTime),
    .io_in_bits_perfDebugInfo_writebackTime(io_in_bits_perfDebugInfo_writebackTime),
    .io_in_bits_perfDebugInfo_runahead_checkpoint_id(io_in_bits_perfDebugInfo_runahead_checkpoint_id),
    .io_in_bits_perfDebugInfo_tlbFirstReqTime(io_in_bits_perfDebugInfo_tlbFirstReqTime),
    .io_in_bits_perfDebugInfo_tlbRespTime(io_in_bits_perfDebugInfo_tlbRespTime),
    .io_out_ready(io_out_ready),
    .io_out_valid(io_out_valid),
    .io_out_bits_ctrl_robIdx_flag(),
    .io_out_bits_ctrl_robIdx_value(),
    .io_out_bits_ctrl_pdest(),
    .io_out_bits_ctrl_vecWen(),
    .io_out_bits_ctrl_v0Wen(),
    .io_out_bits_ctrl_exceptionVec_0(),
    .io_out_bits_ctrl_exceptionVec_1(),
    .io_out_bits_ctrl_exceptionVec_2(),
    .io_out_bits_ctrl_exceptionVec_3(),
    .io_out_bits_ctrl_exceptionVec_4(),
    .io_out_bits_ctrl_exceptionVec_5(),
    .io_out_bits_ctrl_exceptionVec_6(),
    .io_out_bits_ctrl_exceptionVec_7(),
    .io_out_bits_ctrl_exceptionVec_8(),
    .io_out_bits_ctrl_exceptionVec_9(),
    .io_out_bits_ctrl_exceptionVec_10(),
    .io_out_bits_ctrl_exceptionVec_11(),
    .io_out_bits_ctrl_exceptionVec_12(),
    .io_out_bits_ctrl_exceptionVec_13(),
    .io_out_bits_ctrl_exceptionVec_14(),
    .io_out_bits_ctrl_exceptionVec_15(),
    .io_out_bits_ctrl_exceptionVec_16(),
    .io_out_bits_ctrl_exceptionVec_17(),
    .io_out_bits_ctrl_exceptionVec_18(),
    .io_out_bits_ctrl_exceptionVec_19(),
    .io_out_bits_ctrl_exceptionVec_20(),
    .io_out_bits_ctrl_exceptionVec_21(),
    .io_out_bits_ctrl_exceptionVec_22(),
    .io_out_bits_ctrl_exceptionVec_23(),
    .io_out_bits_ctrl_vpu_vill(),
    .io_out_bits_ctrl_vpu_vma(),
    .io_out_bits_ctrl_vpu_vta(),
    .io_out_bits_ctrl_vpu_vsew(),
    .io_out_bits_ctrl_vpu_vlmul(),
    .io_out_bits_ctrl_vpu_specVill(),
    .io_out_bits_ctrl_vpu_specVma(),
    .io_out_bits_ctrl_vpu_specVta(),
    .io_out_bits_ctrl_vpu_specVsew(),
    .io_out_bits_ctrl_vpu_specVlmul(),
    .io_out_bits_ctrl_vpu_vm(),
    .io_out_bits_ctrl_vpu_vstart(),
    .io_out_bits_ctrl_vpu_frm(),
    .io_out_bits_ctrl_vpu_fpu_isFpToVecInst(),
    .io_out_bits_ctrl_vpu_fpu_isFP32Instr(),
    .io_out_bits_ctrl_vpu_fpu_isFP64Instr(),
    .io_out_bits_ctrl_vpu_fpu_isReduction(),
    .io_out_bits_ctrl_vpu_fpu_isFoldTo1_2(),
    .io_out_bits_ctrl_vpu_fpu_isFoldTo1_4(),
    .io_out_bits_ctrl_vpu_fpu_isFoldTo1_8(),
    .io_out_bits_ctrl_vpu_vxrm(),
    .io_out_bits_ctrl_vpu_vuopIdx(),
    .io_out_bits_ctrl_vpu_lastUop(),
    .io_out_bits_ctrl_vpu_vmask(),
    .io_out_bits_ctrl_vpu_vl(),
    .io_out_bits_ctrl_vpu_nf(),
    .io_out_bits_ctrl_vpu_veew(),
    .io_out_bits_ctrl_vpu_isReverse(),
    .io_out_bits_ctrl_vpu_isExt(),
    .io_out_bits_ctrl_vpu_isNarrow(),
    .io_out_bits_ctrl_vpu_isDstMask(),
    .io_out_bits_ctrl_vpu_isOpMask(),
    .io_out_bits_ctrl_vpu_isMove(),
    .io_out_bits_ctrl_vpu_isDependOldVd(),
    .io_out_bits_ctrl_vpu_isWritePartVd(),
    .io_out_bits_ctrl_vpu_isVleff(),
    .io_out_bits_res_data(io_out_bits_res_data),
    .io_out_bits_res_vxsat(),
    .io_out_bits_perfDebugInfo_eliminatedMove(),
    .io_out_bits_perfDebugInfo_renameTime(),
    .io_out_bits_perfDebugInfo_dispatchTime(),
    .io_out_bits_perfDebugInfo_enqRsTime(),
    .io_out_bits_perfDebugInfo_selectTime(),
    .io_out_bits_perfDebugInfo_issueTime(),
    .io_out_bits_perfDebugInfo_writebackTime(),
    .io_out_bits_perfDebugInfo_runahead_checkpoint_id(),
    .io_out_bits_perfDebugInfo_tlbFirstReqTime(),
    .io_out_bits_perfDebugInfo_tlbRespTime(),
    .io_vxrm(io_vxrm)
  );

  always #5 clock = ~clock;

  integer tested = 0;

  task automatic check_dot(
    input logic [127:0] a,
    input logic [127:0] b
  );
    int signed expected;
    int signed ai, bi;
    logic [127:0] expected128;

    expected = 0;
    for (int i = 0; i < 8; i++) begin
      ai = int'($signed(a[i*8 +: 8]));
      bi = int'($signed(b[i*8 +: 8]));
      expected += ai * bi;
    end
    expected128 = {96'b0, expected[31:0]};

    @(negedge clock);
    io_in_valid = 1'b1;
    io_in_bits_ctrl_fuOpType = 9'h078;
    io_in_bits_data_src_0 = a;
    io_in_bits_data_src_1 = b;

    if (!io_in_ready)
      $fatal(1, "Input not ready at test %0d", tested);

    @(posedge clock);
    #1;

    begin
      bit found;
      found = 0;

      for (int cycle = 0; cycle < 5; cycle++) begin
        if (io_out_valid === 1'b1) begin
          found = 1;
          break;
        end

        @(posedge clock);
        #1;
      end

      if (!found)
        $fatal(1,
          "Output not valid after 5 cycles at test %0d", tested);
    end

    $display(
      "DEBUG test=%0d op=%h isVdot=%b vstartGeVl=%b csaResult=%h output=%h",
      tested,
      io_in_bits_ctrl_fuOpType,
      dut.outIsVdot,
      dut.outVstartGeVl,
      dut.outVdotResult,
      io_out_bits_res_data
    );

    if (io_out_bits_res_data !== expected128) begin
      $display("A=%032h B=%032h", a, b);
      $display("Expected=%032h Got=%032h",
               expected128, io_out_bits_res_data);
      $fatal(1, "VDOT mismatch at test %0d", tested);
    end
    tested++;
  endtask

  logic [127:0] a, b;

  initial begin
    clock = '0;
    reset = '0;
    io_flush_valid = '0;
    io_flush_bits_isRVC = '0;
    io_flush_bits_robIdx_flag = '0;
    io_flush_bits_robIdx_value = '0;
    io_flush_bits_ftqIdx_flag = '0;
    io_flush_bits_ftqIdx_value = '0;
    io_flush_bits_ftqOffset = '0;
    io_flush_bits_level = '0;
    io_flush_bits_interrupt = '0;
    io_flush_bits_cfiUpdate_pc = '0;
    io_flush_bits_cfiUpdate_pd_valid = '0;
    io_flush_bits_cfiUpdate_pd_isRVC = '0;
    io_flush_bits_cfiUpdate_pd_brType = '0;
    io_flush_bits_cfiUpdate_pd_isCall = '0;
    io_flush_bits_cfiUpdate_pd_isRet = '0;
    io_flush_bits_cfiUpdate_ssp = '0;
    io_flush_bits_cfiUpdate_sctr = '0;
    io_flush_bits_cfiUpdate_TOSW_flag = '0;
    io_flush_bits_cfiUpdate_TOSW_value = '0;
    io_flush_bits_cfiUpdate_TOSR_flag = '0;
    io_flush_bits_cfiUpdate_TOSR_value = '0;
    io_flush_bits_cfiUpdate_NOS_flag = '0;
    io_flush_bits_cfiUpdate_NOS_value = '0;
    io_flush_bits_cfiUpdate_topAddr = '0;
    io_flush_bits_cfiUpdate_folded_hist_hist_17_folded_hist = '0;
    io_flush_bits_cfiUpdate_folded_hist_hist_16_folded_hist = '0;
    io_flush_bits_cfiUpdate_folded_hist_hist_15_folded_hist = '0;
    io_flush_bits_cfiUpdate_folded_hist_hist_14_folded_hist = '0;
    io_flush_bits_cfiUpdate_folded_hist_hist_13_folded_hist = '0;
    io_flush_bits_cfiUpdate_folded_hist_hist_12_folded_hist = '0;
    io_flush_bits_cfiUpdate_folded_hist_hist_11_folded_hist = '0;
    io_flush_bits_cfiUpdate_folded_hist_hist_10_folded_hist = '0;
    io_flush_bits_cfiUpdate_folded_hist_hist_9_folded_hist = '0;
    io_flush_bits_cfiUpdate_folded_hist_hist_8_folded_hist = '0;
    io_flush_bits_cfiUpdate_folded_hist_hist_7_folded_hist = '0;
    io_flush_bits_cfiUpdate_folded_hist_hist_6_folded_hist = '0;
    io_flush_bits_cfiUpdate_folded_hist_hist_5_folded_hist = '0;
    io_flush_bits_cfiUpdate_folded_hist_hist_4_folded_hist = '0;
    io_flush_bits_cfiUpdate_folded_hist_hist_3_folded_hist = '0;
    io_flush_bits_cfiUpdate_folded_hist_hist_2_folded_hist = '0;
    io_flush_bits_cfiUpdate_folded_hist_hist_1_folded_hist = '0;
    io_flush_bits_cfiUpdate_folded_hist_hist_0_folded_hist = '0;
    io_flush_bits_cfiUpdate_afhob_afhob_5_bits_0 = '0;
    io_flush_bits_cfiUpdate_afhob_afhob_5_bits_1 = '0;
    io_flush_bits_cfiUpdate_afhob_afhob_5_bits_2 = '0;
    io_flush_bits_cfiUpdate_afhob_afhob_5_bits_3 = '0;
    io_flush_bits_cfiUpdate_afhob_afhob_4_bits_0 = '0;
    io_flush_bits_cfiUpdate_afhob_afhob_4_bits_1 = '0;
    io_flush_bits_cfiUpdate_afhob_afhob_4_bits_2 = '0;
    io_flush_bits_cfiUpdate_afhob_afhob_4_bits_3 = '0;
    io_flush_bits_cfiUpdate_afhob_afhob_3_bits_0 = '0;
    io_flush_bits_cfiUpdate_afhob_afhob_3_bits_1 = '0;
    io_flush_bits_cfiUpdate_afhob_afhob_3_bits_2 = '0;
    io_flush_bits_cfiUpdate_afhob_afhob_3_bits_3 = '0;
    io_flush_bits_cfiUpdate_afhob_afhob_2_bits_0 = '0;
    io_flush_bits_cfiUpdate_afhob_afhob_2_bits_1 = '0;
    io_flush_bits_cfiUpdate_afhob_afhob_2_bits_2 = '0;
    io_flush_bits_cfiUpdate_afhob_afhob_2_bits_3 = '0;
    io_flush_bits_cfiUpdate_afhob_afhob_1_bits_0 = '0;
    io_flush_bits_cfiUpdate_afhob_afhob_1_bits_1 = '0;
    io_flush_bits_cfiUpdate_afhob_afhob_1_bits_2 = '0;
    io_flush_bits_cfiUpdate_afhob_afhob_1_bits_3 = '0;
    io_flush_bits_cfiUpdate_afhob_afhob_0_bits_0 = '0;
    io_flush_bits_cfiUpdate_afhob_afhob_0_bits_1 = '0;
    io_flush_bits_cfiUpdate_afhob_afhob_0_bits_2 = '0;
    io_flush_bits_cfiUpdate_afhob_afhob_0_bits_3 = '0;
    io_flush_bits_cfiUpdate_lastBrNumOH = '0;
    io_flush_bits_cfiUpdate_ghr = '0;
    io_flush_bits_cfiUpdate_histPtr_flag = '0;
    io_flush_bits_cfiUpdate_histPtr_value = '0;
    io_flush_bits_cfiUpdate_specCnt_0 = '0;
    io_flush_bits_cfiUpdate_specCnt_1 = '0;
    io_flush_bits_cfiUpdate_br_hit = '0;
    io_flush_bits_cfiUpdate_jr_hit = '0;
    io_flush_bits_cfiUpdate_sc_hit = '0;
    io_flush_bits_cfiUpdate_predTaken = '0;
    io_flush_bits_cfiUpdate_target = '0;
    io_flush_bits_cfiUpdate_taken = '0;
    io_flush_bits_cfiUpdate_isMisPred = '0;
    io_flush_bits_cfiUpdate_shift = '0;
    io_flush_bits_cfiUpdate_addIntoHist = '0;
    io_flush_bits_cfiUpdate_backendIGPF = '0;
    io_flush_bits_cfiUpdate_backendIPF = '0;
    io_flush_bits_cfiUpdate_backendIAF = '0;
    io_flush_bits_fullTarget = '0;
    io_flush_bits_stFtqIdx_flag = '0;
    io_flush_bits_stFtqIdx_value = '0;
    io_flush_bits_stFtqOffset = '0;
    io_flush_bits_debug_runahead_checkpoint_id = '0;
    io_flush_bits_debugIsCtrl = '0;
    io_flush_bits_debugIsMemVio = '0;
    io_in_valid = '0;
    io_in_bits_ctrl_fuOpType = '0;
    io_in_bits_ctrl_robIdx_flag = '0;
    io_in_bits_ctrl_robIdx_value = '0;
    io_in_bits_ctrl_pdest = '0;
    io_in_bits_ctrl_vecWen = '0;
    io_in_bits_ctrl_v0Wen = '0;
    io_in_bits_ctrl_vpu_vill = '0;
    io_in_bits_ctrl_vpu_vma = '0;
    io_in_bits_ctrl_vpu_vta = '0;
    io_in_bits_ctrl_vpu_vsew = '0;
    io_in_bits_ctrl_vpu_vlmul = '0;
    io_in_bits_ctrl_vpu_specVill = '0;
    io_in_bits_ctrl_vpu_specVma = '0;
    io_in_bits_ctrl_vpu_specVta = '0;
    io_in_bits_ctrl_vpu_specVsew = '0;
    io_in_bits_ctrl_vpu_specVlmul = '0;
    io_in_bits_ctrl_vpu_vm = '0;
    io_in_bits_ctrl_vpu_vstart = '0;
    io_in_bits_ctrl_vpu_frm = '0;
    io_in_bits_ctrl_vpu_fpu_isFpToVecInst = '0;
    io_in_bits_ctrl_vpu_fpu_isFP32Instr = '0;
    io_in_bits_ctrl_vpu_fpu_isFP64Instr = '0;
    io_in_bits_ctrl_vpu_fpu_isReduction = '0;
    io_in_bits_ctrl_vpu_fpu_isFoldTo1_2 = '0;
    io_in_bits_ctrl_vpu_fpu_isFoldTo1_4 = '0;
    io_in_bits_ctrl_vpu_fpu_isFoldTo1_8 = '0;
    io_in_bits_ctrl_vpu_vxrm = '0;
    io_in_bits_ctrl_vpu_vuopIdx = '0;
    io_in_bits_ctrl_vpu_lastUop = '0;
    io_in_bits_ctrl_vpu_vmask = '0;
    io_in_bits_ctrl_vpu_vl = '0;
    io_in_bits_ctrl_vpu_nf = '0;
    io_in_bits_ctrl_vpu_veew = '0;
    io_in_bits_ctrl_vpu_isReverse = '0;
    io_in_bits_ctrl_vpu_isExt = '0;
    io_in_bits_ctrl_vpu_isNarrow = '0;
    io_in_bits_ctrl_vpu_isDstMask = '0;
    io_in_bits_ctrl_vpu_isOpMask = '0;
    io_in_bits_ctrl_vpu_isMove = '0;
    io_in_bits_ctrl_vpu_isDependOldVd = '0;
    io_in_bits_ctrl_vpu_isWritePartVd = '0;
    io_in_bits_ctrl_vpu_isVleff = '0;
    io_in_bits_ctrlPipe_0_fuOpType = '0;
    io_in_bits_ctrlPipe_0_robIdx_flag = '0;
    io_in_bits_ctrlPipe_0_robIdx_value = '0;
    io_in_bits_ctrlPipe_0_pdest = '0;
    io_in_bits_ctrlPipe_0_vecWen = '0;
    io_in_bits_ctrlPipe_0_v0Wen = '0;
    io_in_bits_ctrlPipe_0_vpu_vill = '0;
    io_in_bits_ctrlPipe_0_vpu_vma = '0;
    io_in_bits_ctrlPipe_0_vpu_vta = '0;
    io_in_bits_ctrlPipe_0_vpu_vsew = '0;
    io_in_bits_ctrlPipe_0_vpu_vlmul = '0;
    io_in_bits_ctrlPipe_0_vpu_specVill = '0;
    io_in_bits_ctrlPipe_0_vpu_specVma = '0;
    io_in_bits_ctrlPipe_0_vpu_specVta = '0;
    io_in_bits_ctrlPipe_0_vpu_specVsew = '0;
    io_in_bits_ctrlPipe_0_vpu_specVlmul = '0;
    io_in_bits_ctrlPipe_0_vpu_vm = '0;
    io_in_bits_ctrlPipe_0_vpu_vstart = '0;
    io_in_bits_ctrlPipe_0_vpu_frm = '0;
    io_in_bits_ctrlPipe_0_vpu_fpu_isFpToVecInst = '0;
    io_in_bits_ctrlPipe_0_vpu_fpu_isFP32Instr = '0;
    io_in_bits_ctrlPipe_0_vpu_fpu_isFP64Instr = '0;
    io_in_bits_ctrlPipe_0_vpu_fpu_isReduction = '0;
    io_in_bits_ctrlPipe_0_vpu_fpu_isFoldTo1_2 = '0;
    io_in_bits_ctrlPipe_0_vpu_fpu_isFoldTo1_4 = '0;
    io_in_bits_ctrlPipe_0_vpu_fpu_isFoldTo1_8 = '0;
    io_in_bits_ctrlPipe_0_vpu_vxrm = '0;
    io_in_bits_ctrlPipe_0_vpu_vuopIdx = '0;
    io_in_bits_ctrlPipe_0_vpu_lastUop = '0;
    io_in_bits_ctrlPipe_0_vpu_vmask = '0;
    io_in_bits_ctrlPipe_0_vpu_vl = '0;
    io_in_bits_ctrlPipe_0_vpu_nf = '0;
    io_in_bits_ctrlPipe_0_vpu_veew = '0;
    io_in_bits_ctrlPipe_0_vpu_isReverse = '0;
    io_in_bits_ctrlPipe_0_vpu_isExt = '0;
    io_in_bits_ctrlPipe_0_vpu_isNarrow = '0;
    io_in_bits_ctrlPipe_0_vpu_isDstMask = '0;
    io_in_bits_ctrlPipe_0_vpu_isOpMask = '0;
    io_in_bits_ctrlPipe_0_vpu_isMove = '0;
    io_in_bits_ctrlPipe_0_vpu_isDependOldVd = '0;
    io_in_bits_ctrlPipe_0_vpu_isWritePartVd = '0;
    io_in_bits_ctrlPipe_0_vpu_isVleff = '0;
    io_in_bits_ctrlPipe_1_fuOpType = '0;
    io_in_bits_ctrlPipe_1_robIdx_flag = '0;
    io_in_bits_ctrlPipe_1_robIdx_value = '0;
    io_in_bits_ctrlPipe_1_pdest = '0;
    io_in_bits_ctrlPipe_1_vecWen = '0;
    io_in_bits_ctrlPipe_1_v0Wen = '0;
    io_in_bits_ctrlPipe_1_vpu_vill = '0;
    io_in_bits_ctrlPipe_1_vpu_vma = '0;
    io_in_bits_ctrlPipe_1_vpu_vta = '0;
    io_in_bits_ctrlPipe_1_vpu_vsew = '0;
    io_in_bits_ctrlPipe_1_vpu_vlmul = '0;
    io_in_bits_ctrlPipe_1_vpu_specVill = '0;
    io_in_bits_ctrlPipe_1_vpu_specVma = '0;
    io_in_bits_ctrlPipe_1_vpu_specVta = '0;
    io_in_bits_ctrlPipe_1_vpu_specVsew = '0;
    io_in_bits_ctrlPipe_1_vpu_specVlmul = '0;
    io_in_bits_ctrlPipe_1_vpu_vm = '0;
    io_in_bits_ctrlPipe_1_vpu_vstart = '0;
    io_in_bits_ctrlPipe_1_vpu_frm = '0;
    io_in_bits_ctrlPipe_1_vpu_fpu_isFpToVecInst = '0;
    io_in_bits_ctrlPipe_1_vpu_fpu_isFP32Instr = '0;
    io_in_bits_ctrlPipe_1_vpu_fpu_isFP64Instr = '0;
    io_in_bits_ctrlPipe_1_vpu_fpu_isReduction = '0;
    io_in_bits_ctrlPipe_1_vpu_fpu_isFoldTo1_2 = '0;
    io_in_bits_ctrlPipe_1_vpu_fpu_isFoldTo1_4 = '0;
    io_in_bits_ctrlPipe_1_vpu_fpu_isFoldTo1_8 = '0;
    io_in_bits_ctrlPipe_1_vpu_vxrm = '0;
    io_in_bits_ctrlPipe_1_vpu_vuopIdx = '0;
    io_in_bits_ctrlPipe_1_vpu_lastUop = '0;
    io_in_bits_ctrlPipe_1_vpu_vmask = '0;
    io_in_bits_ctrlPipe_1_vpu_vl = '0;
    io_in_bits_ctrlPipe_1_vpu_nf = '0;
    io_in_bits_ctrlPipe_1_vpu_veew = '0;
    io_in_bits_ctrlPipe_1_vpu_isReverse = '0;
    io_in_bits_ctrlPipe_1_vpu_isExt = '0;
    io_in_bits_ctrlPipe_1_vpu_isNarrow = '0;
    io_in_bits_ctrlPipe_1_vpu_isDstMask = '0;
    io_in_bits_ctrlPipe_1_vpu_isOpMask = '0;
    io_in_bits_ctrlPipe_1_vpu_isMove = '0;
    io_in_bits_ctrlPipe_1_vpu_isDependOldVd = '0;
    io_in_bits_ctrlPipe_1_vpu_isWritePartVd = '0;
    io_in_bits_ctrlPipe_1_vpu_isVleff = '0;
    io_in_bits_validPipe_0 = '0;
    io_in_bits_validPipe_1 = '0;
    io_in_bits_data_src_4 = '0;
    io_in_bits_data_src_3 = '0;
    io_in_bits_data_src_2 = '0;
    io_in_bits_data_src_1 = '0;
    io_in_bits_data_src_0 = '0;
    io_in_bits_data_imm = '0;
    io_in_bits_dataPipe_0_src_4 = '0;
    io_in_bits_dataPipe_0_src_3 = '0;
    io_in_bits_dataPipe_0_src_2 = '0;
    io_in_bits_dataPipe_0_src_1 = '0;
    io_in_bits_dataPipe_0_src_0 = '0;
    io_in_bits_dataPipe_0_imm = '0;
    io_in_bits_dataPipe_1_src_4 = '0;
    io_in_bits_dataPipe_1_src_3 = '0;
    io_in_bits_dataPipe_1_src_2 = '0;
    io_in_bits_dataPipe_1_src_1 = '0;
    io_in_bits_dataPipe_1_src_0 = '0;
    io_in_bits_dataPipe_1_imm = '0;
    io_in_bits_perfDebugInfo_eliminatedMove = '0;
    io_in_bits_perfDebugInfo_renameTime = '0;
    io_in_bits_perfDebugInfo_dispatchTime = '0;
    io_in_bits_perfDebugInfo_enqRsTime = '0;
    io_in_bits_perfDebugInfo_selectTime = '0;
    io_in_bits_perfDebugInfo_issueTime = '0;
    io_in_bits_perfDebugInfo_writebackTime = '0;
    io_in_bits_perfDebugInfo_runahead_checkpoint_id = '0;
    io_in_bits_perfDebugInfo_tlbFirstReqTime = '0;
    io_in_bits_perfDebugInfo_tlbRespTime = '0;
    io_out_ready = '0;
    io_vxrm = '0;

    clock = 0;
    reset = 1;
    io_out_ready = 1;
    // Pipeline control signals used by the output mux.
    io_in_bits_ctrlPipe_1_vpu_vstart = 8'd0;
    io_in_bits_dataPipe_1_src_4 = 8'd8;
    io_in_bits_ctrlPipe_1_vpu_vl = 8'd8;

    io_in_bits_validPipe_0 = 1'b1;
    io_in_bits_validPipe_1 = 1'b1;
    io_in_bits_ctrl_vecWen = 1'b1;
    io_in_bits_ctrl_vpu_vm = 1'b1;
    io_in_bits_ctrl_vpu_vsew = 2'b00;
    io_in_bits_ctrl_vpu_vlmul = 3'b000;
    io_in_bits_ctrl_vpu_vstart = 0;
    io_in_bits_ctrl_vpu_vl = 8;
    io_in_bits_ctrl_vpu_lastUop = 1'b1;

    repeat (3) @(posedge clock);
    @(negedge clock);
    reset = 0;

    check_dot('0, '0);
    check_dot({16{8'h7f}}, {16{8'h7f}});
    check_dot({16{8'h80}}, {16{8'h80}});
    check_dot({16{8'h7f}}, {16{8'h80}});
    check_dot(128'h0123456789abcdef0123456789abcdef,
              128'hfedcba9876543210fedcba9876543210);

    for (int t = 0; t < 100000; t++) begin
      for (int k = 0; k < 4; k++) begin
        a[k*32 +: 32] = $urandom;
        b[k*32 +: 32] = $urandom;
      end
      check_dot(a, b);
    end

    $display("PASS: %0d actual VIAluFix RTL tests", tested);
    $finish;
  end

endmodule
