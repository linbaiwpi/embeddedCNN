/*
    Desc:
    
        CNN top in Xilinx FPGA.

    Note:
        
        Platform: ZCU102
        Toolkit: SDx 2018.1

    Date:

        06/08/2018

    Author:

        Yue Niu
*/
#include <iostream>
#include <fstream>
#include <cmath>
#include <stdlib.h>

#include "sds_lib.h"
#include "hls_half.h"

#include "../../include/fpga/cnn_fpga.h"
#include "../../include/fpga/conv_fpga.h"
#include "../../include/fpga/fc_fpga.h"
#include "../../include/data/get_param.h"
#include "../include/utils/check.h"
#include "../include/utils/performance.h"
#include "../include/common.h"

extern const int SHAPE[18];
extern const int CHNEL[18];
extern const int KERNL[13];
extern const bool POOL[13];

/* 
  Desc:

    Top cnn module in Xilinx FPGA platform.
    This module takes input RGB image, outputing 1000 classification 
    results.

  Argument:

    In, pointer to input RGB data.
    Out, pointer to output classification result.

  Note:

*/
void cnn_fpga(Dtype *In, Dtype *Out, Dtype *Params)
{
  std::cout << "[INFO] " << __FUNCTION__ << ", " << __LINE__ <<
               ": Start CNN in Xilinx FPGA..." << std::endl;

  // Set two ping-pang memory for layer input and output
  // buf size should meet maximum memory requirement for
  // each layer in CNN
  int max_buf_size = SHAPE[0] * SHAPE[0] * CHNEL[0];
  Dtype *bufferA = (Dtype *)sds_alloc(max_buf_size * sizeof(Dtype));
  Dtype *bufferB = (Dtype *)sds_alloc(max_buf_size * sizeof(Dtype));
  mem_check(bufferA); mem_check(bufferB);

  /* Do conv layer by layer */
  // Set a pingpang memory flag
  // 0: bufferA/In(input), bufferB(output)
  // 1: bufferA(output), bufferB(input)
  int pingpang = 0;
  Dtype *cur_params = Params;
  float total_time = 0;
  for (int c_layer = 0; c_layer < CONV_LAYER_NUM; c_layer++)
  {
    int w_isec = 0; // 2^w_isec
    int til_num = 0;
    switch (c_layer){
      case 0:  {w_isec = 0; til_num = 112; break;}
      case 1:  {w_isec = 2; til_num = 112; break;}
      case 2:  {w_isec = 2; til_num = 19; break;}
      case 3:  {w_isec = 3; til_num = 19; break;}
      case 4:  {w_isec = 3; til_num = 4;break;}
      case 5:  {w_isec = 4; til_num = 4;break;}
      case 6:  {w_isec = 4; til_num = 4;break;}
      case 7:  {w_isec = 3; til_num = 1;break;}
      case 8:  {w_isec = 3; til_num = 1;break;}
      case 9:  {w_isec = 3; til_num = 1;break;}
      case 10: {w_isec = 3; til_num = 1;break;}
      case 11: {w_isec = 3; til_num = 1;break;}
      case 12: {w_isec = 3; til_num = 1;break;} 
      default: w_isec = 0; break;
    }
    int col_num = SHAPE[c_layer];
    int row_num = SHAPE[c_layer];
    if (0 == c_layer){
      std::cout << "[INFO] " << __FUNCTION__ << ", " << __LINE__ <<
                  ": " << c_layer << "th convolution layer." << std::endl;
      int chnl_to_read = 3;
      int isec = 1;
      int osec = CHNEL[c_layer] / OTILE;
      int pool_div = 1;
      if (POOL[c_layer]) pool_div = 4;
      else               pool_div = 1;
      perf_counter perf;
      perf.start();
      conv_fpga(In, 
                cur_params,
                bufferB,
                c_layer, 
                row_num, 
                col_num, 
                KERNL[c_layer], 
                chnl_to_read, 
                isec, 
                CHNEL[c_layer], 
                osec, 
                til_num,
                w_isec,
                pool_div,
                POOL[c_layer]);
      perf.stop();
      uint64_t cpu_cycles = perf.avg_cpu_cycles();
      float lyr_time = (float)cpu_cycles / 1.5e9;
      total_time += lyr_time;
      std::cout << "[INFO] " << __FUNCTION__ << ", " << __LINE__ <<
                  ": Finish in " << lyr_time << "s." << std::endl;
      cur_params += (CHNEL[0] * 3 * KERNL[0] * KERNL[0] + CHNEL[0]);
      pingpang = 1;
    }
    else { /* c_layer != 0 */
      //int chnl_to_read = ITILE;
      int isec = CHNEL[c_layer - 1] / ITILE;
      int osec = CHNEL[c_layer] / OTILE;
      int pool_div = 1;
      if (POOL[c_layer]) pool_div = 4;
      else               pool_div = 1;
      std::cout << "[INFO] " << __FUNCTION__ << ", " << __LINE__ <<
                 ": " << c_layer << "th convolution layer." << std::endl;
      perf_counter perf;
      if (0 == pingpang)
      {
        perf.start();

        //char *bufferA_char = reinterpret_cast<char *>(bufferA);
        //std::ifstream input("./data/conv5_2fp16.bin", std::ios::binary);
        //input.read(bufferA_char, SHAPE[c_layer-1] * SHAPE[c_layer-1]*CHNEL[c_layer-1]*sizeof(Dtype));
        //input.close();
        //cur_params += 12354880;

        conv_fpga(bufferA, 
                  cur_params,
                  bufferB,
                  c_layer, 
                  row_num, 
                  col_num, 
                  KERNL[c_layer], 
                  CHNEL[c_layer - 1], 
                  isec, 
                  CHNEL[c_layer], 
                  osec, 
                  til_num,
                  w_isec,
                  pool_div,
                  POOL[c_layer]);
        perf.stop();
        pingpang = 1;
      }
      else 
      {
        perf.start();
        conv_fpga(bufferB, 
                  cur_params,
                  bufferA,
                  c_layer, 
                  row_num, 
                  col_num, 
                  KERNL[c_layer], 
                  CHNEL[c_layer - 1], 
                  isec, 
                  CHNEL[c_layer], 
                  osec, 
                  til_num,
                  w_isec,
                  pool_div,
                  POOL[c_layer]);
        perf.stop();
        pingpang = 0;
      }
      uint64_t cpu_cycles = perf.avg_cpu_cycles();
      float lyr_time = (float)cpu_cycles / 1.5e9;
      total_time += lyr_time;
      std::cout << "[INFO] " << __FUNCTION__ << ", " << __LINE__ <<
                  ": Finish in " << lyr_time << "s." << std::endl;

      // Update pointer to parameters
      cur_params += (CHNEL[c_layer] * CHNEL[c_layer - 1] * KERNL[0] * KERNL[0] +
                     CHNEL[c_layer]);
    } /* c_layer != 0 */

    #ifdef CHECK_FPGA_CONV
    if (CONV_LAYER_NUM-1 == c_layer){
      Dtype *buffer_ptr = pingpang == 0 ? bufferA : bufferB;
      std::cout << "[INFO] " << __FUNCTION__ << ", " << __LINE__ << 
                   ": Check On-chip data." << std::endl;
      computing_check(buffer_ptr, c_layer, POOL[c_layer]);
    }
    #endif
  }
  std::cout << "[INFO] " << __FUNCTION__ << ", " << __LINE__ << 
               ": Total time in conv layer, " << total_time << "s." << std::endl;

  /* Do FC layer by layer */
  Dtype *fc_input = 0 == pingpang ? bufferA : bufferB;
  //std::ifstream input_handle("./data/pool5fp16.bin", std::ios::binary);
  //Dtype *fc_input = (Dtype *) sds_alloc(25088 * sizeof(Dtype));
  //mem_check(fc_input);
  //char *fc_input_char = reinterpret_cast<char *>(fc_input);
  //input_handle.read(fc_input_char, 25088 * sizeof(Dtype));
  //input_handle.close();
  //cur_params += 14714688;
  perf_counter perf;
  std::cout << "[INFO] " << __FUNCTION__ << ", " << __LINE__ <<
               ": FC layer." << std::endl;
  perf.start();
  fc_fpga(fc_input, cur_params, Out);
  perf.stop();
  uint64_t cpu_cycles = perf.avg_cpu_cycles();
  float fc_time = (float)cpu_cycles / 1.5e9;
  std::cout << "[INFO] " << __FUNCTION__ << ", " << __LINE__ <<
               ": Finish in " << fc_time << "s." << std::endl;
  #ifdef CHECK_FPGA_FC
  fc_check(Out, FC_LAYER_NUM - 1);
  #endif

  // Free memory
  sds_free(bufferA); sds_free(bufferB);
  return;
}
