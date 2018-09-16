#include "SRNNPredictor.hpp"
#define DPRINT(fmt, ...) printf("%s(%d)debug:" fmt, __func__, __LINE__, ## __VA_ARGS__)

#define PRINT_VEC(ary) \
  for (int iii=0; iii < MATRIX_LENGTH(ary); iii++) { \
    printf("%d %f \n", iii, ary[iii]); \
  }

/* コンストラクタ - 最小の初期化パラメタ
 * 適宜追加する可能性あり
 */
SRNNPredictor::SRNNPredictor(int dim_in_signal, 
                             int dim_out_signal,
                             int num_mid)
{
  this->dim_in_signal  = dim_in_signal;
  this->dim_out_signal = dim_out_signal;
  this->num_mid_neuron = num_mid; // こいつが一番ネック.学習能力に関わる

  
  // 係数行列のアロケート
  // 最後の+1はバイアス出力のためにある.
  this->Win_mid  = new float[num_mid_neuron * (dim_in_signal + num_mid_neuron + 1)]; 
  this->Wmid_out = new float[dim_out_signal * (num_mid_neuron + 1)];

  // 入力/中間層のアロケート
  expand_in_signal = new float[dim_in_signal + num_mid_neuron + 1];
  expand_mid_signal = new float[num_mid_neuron + 1];

  // 作者の偏見によるパラメタ設定. 頻繁に変えるものは後に変えられるように変更する.
  this->squareError    = FLT_MAX; // 大きな値ならなんでも良い ん？
  this->maxIteration   = 10000;
  this->goalError      = float(1e-7);
  this->epsilon        = float(1e-7);
  this->learnRate      = float(0.9);
  this->alpha          = float(0.8 * learnRate);
  this->alpha_context  = float(0.0);
  this->width_initW    = float(float(1.0)/num_mid_neuron);

  srand((unsigned int)time(NULL));

}

void SRNNPredictor::sigmoid_vec(float* net,
                                float* out,
                                int    dim)
{
  for (int n = 0;n < dim;n++) {
    out[n] = sigmoid_func(net[n]); // sigmoid_funcはシグモイド関数の計算.
  }
}

int SRNNPredictor::set_sample(float* sample, float* label,
    float* sample_maxmin, int len_seqence, int dim_sample, int dim_label)
{
  if (dim_sample != this->dim_in_signal) {
    // fprintf("Sample dimention must be equal with SRNN predictor.\n");
    return -1;
  }
  if (dim_label != this->dim_out_signal) {
    // fprintf("Sample dimention must be equal with SRNN predictor.\n");
    return -2;
  }

  this->len_seqence    = len_seqence;

  // TODO:廃止予定
  // sample/sample_maxminのアロケート
  this->sample = new float[len_seqence * dim_sample];
  this->sample_maxmin = new float[dim_sample * 2];
  this->label = new float[len_seqence * dim_label];

  memcpy(this->sample, sample, sizeof(float) * len_seqence * dim_in_signal);
  memcpy(this->label,   label, sizeof(float) * len_seqence * dim_out_signal);
  memcpy(this->sample_maxmin, sample_maxmin, sizeof(float) * dim_sample * 2);

  return 0;
}

/* 予測: inputの次の系列を学習結果から予測 */
void SRNNPredictor::predict(float* input, float* predict)
{
  float *norm_input = new float[this->dim_in_signal];
  // 係数行列のサイズ

  // 信号の正規化
  for (int n=0; n < dim_in_signal; n++) {
    norm_input[n] = 
      normalize_signal(input[n],
          MATRIX_AT(this->sample_maxmin,2,n,0),
          MATRIX_AT(this->sample_maxmin,2,n,1));
  }

  // 出力層の信号
  float* out_signal = new float[dim_out_signal];
  // 入力層->中間層の信号和
  float* in_mid_net = new float[num_mid_neuron];
  // 中間層->出力層の信号和.
  float* mid_out_net = new float[dim_in_signal];

  /* 出力信号を一旦計算 */
  // 入力値を取得
  memcpy(expand_in_signal, norm_input, sizeof(float) * dim_in_signal);
  // 入力層の信号
  // 中間層との線形和をシグモイド関数に通す.
  for (int d = 0; d < num_mid_neuron; d++) {
    expand_in_signal[dim_in_signal + d] = sigmoid_func(alpha_context * expand_in_signal[dim_in_signal + d] + expand_mid_signal[d]);
  }
  // バイアス項は常に1に固定.
  expand_in_signal[dim_in_signal + num_mid_neuron] = 1;

  // 入力->中間層の出力信号和計算
  multiply_mat_vec(Win_mid, expand_in_signal, in_mid_net, num_mid_neuron, dim_in_signal + num_mid_neuron + 1);
  // 中間層の出力信号計算
  sigmoid_vec(in_mid_net, expand_mid_signal, num_mid_neuron);
  expand_mid_signal[num_mid_neuron] = 1;

  // 中間->出力層の出力信号和計算
  multiply_mat_vec(Wmid_out, expand_mid_signal, mid_out_net, dim_out_signal, num_mid_neuron + 1);
  // 出力層の出力信号計算
  sigmoid_vec(mid_out_net, out_signal, dim_out_signal);

  // 出力信号を,元の信号に拡張.
  for (int n=0;n < dim_out_signal;n++) {
    predict[n] = expand_signal(out_signal[n],sample_maxmin[n * 2],sample_maxmin[n * 2 + 1]);
  }
}

/* 逆誤差伝搬法による学習 局所解？なんのこったよ（すっとぼけ）*/
float SRNNPredictor::learning(void)
{
  int iteration = 0; // 学習繰り返し回数
  int seq = 0;       // 現在学習中の系列番号[0,...,len_seqence-1]
  // 係数行列のサイズ
  int row_in_mid = num_mid_neuron;
  int col_in_mid = dim_in_signal + num_mid_neuron + 1;
  int row_mid_out = dim_out_signal;
  int col_mid_out = num_mid_neuron + 1;

  // 行列のアロケート
  // 係数行列の更新量
  float* dWin_mid  = new float[row_in_mid * col_in_mid];
  float* dWmid_out = new float[row_mid_out * col_mid_out];
  // 前回の更新量:慣性項に用いる.
  float* prevdWin_mid  = new float[row_in_mid * col_in_mid];
  float* prevdWmid_out = new float[row_mid_out * col_mid_out];
  float* norm_sample   = new float[len_seqence * dim_in_signal]; // 正規化したサンプル信号; 実際の学習は正規化した信号を用います.
  float* norm_label    = new float[len_seqence * dim_out_signal]; // 正規化したラベル

  // 係数行列の初期化
  for (int i=0; i < row_in_mid; i++)
    for (int j=0; j < col_in_mid; j++)
      MATRIX_AT(Win_mid,col_in_mid,i,j) = uniform_rand(width_initW);

  for (int i=0; i < row_mid_out; i++)
    for (int j=0; j < col_mid_out; j++)
      MATRIX_AT(Wmid_out,col_mid_out,i,j) = uniform_rand(width_initW);

  // 信号の正規化:経験上,非常に大切な処理
  for (int seq=0; seq < len_seqence; seq++) {
    for (int n=0; n < dim_in_signal; n++) {
      MATRIX_AT(norm_sample,dim_in_signal,seq,n) = 
            normalize_signal(MATRIX_AT(this->sample,dim_in_signal,seq,n),
                             MATRIX_AT(this->sample_maxmin,2,n,0),
                             MATRIX_AT(this->sample_maxmin,2,n,1));
    }
    // ラベルについても同様
    for (int n=0; n < dim_out_signal; n++) {
      MATRIX_AT(norm_label,dim_out_signal,seq,n) = 
            normalize_signal(MATRIX_AT(this->label,dim_out_signal,seq,n),
                             MATRIX_AT(this->sample_maxmin,2,n,0),
                             MATRIX_AT(this->sample_maxmin,2,n,1));
    }
  }

  // 出力層の信号
  float* out_signal = new float[dim_out_signal];

  // 入力層->中間層の信号和
  float* in_mid_net = new float[num_mid_neuron];
  // 中間層->出力層の信号和.
  float* mid_out_net = new float[dim_out_signal];

  // 誤差信号
  float* sigma = new float[dim_out_signal];

  // 前回の二乗誤差値:収束判定に用いる.
  float prevError = FLT_MAX;

  /* 学習ループ */
  while (1) {

    if (iteration > 0) {
      printf("ite:%d err:%f \n", iteration, squareError);
    }

    // 二乗誤差
    squareError = 0;
    for (seq = 0; seq < len_seqence; seq++) {

      // 前回の更新量/二乗誤差を保存
      if (iteration >= 1) {
        memcpy(prevdWin_mid, dWin_mid, sizeof(float) * row_in_mid * col_in_mid);
        memcpy(prevdWmid_out, dWmid_out, sizeof(float) * row_mid_out * col_mid_out);
      } else {
        memset(prevdWin_mid, float(0), sizeof(float) * row_in_mid * col_in_mid);
        memset(prevdWmid_out, float(0), sizeof(float) * row_mid_out * col_mid_out);
      }

      /* 行列表示
         for (int i = 0; i < row_mid_out; i++) {
         for (int j = 0; j < col_mid_out; j++) {
         if (j == 0) { printf("|"); }
         float val = MATRIX_AT(Wmid_out,col_mid_out,i,j);
         if (val > 0) { printf(" "); }
         printf("%3.3f ",val);
         if (j == col_mid_out-1) { printf("|\n"); };
         }
         }
         */

      /* 学習ステップその1:ニューラルネットの出力信号を求める */

      // 入力値を取得
      memcpy(expand_in_signal, &(norm_sample[seq * dim_in_signal]), sizeof(float) * dim_in_signal);
      // SRNN特有:入力層に中間層のコピーが追加され,中間層に入力される.
      if (iteration == 0) {
        // 初回は0埋めする
        memset(&(expand_in_signal[dim_in_signal]), float(0), sizeof(float) * num_mid_neuron);
      } else {
        // コンテキスト層 = 前回のコンテキスト層の出力
        // 前回の中間層信号との線形和をシグモイド関数に通す.
        for (int d = 0; d < num_mid_neuron; d++) {
          expand_in_signal[dim_in_signal + d] = sigmoid_func(alpha_context * expand_in_signal[dim_in_signal + d] + expand_mid_signal[d]);
        }
      }
      // バイアス項は常に1に固定.
      expand_in_signal[dim_in_signal + num_mid_neuron] = 1;

      // 入力->中間層の出力信号和計算
      multiply_mat_vec(Win_mid,
          expand_in_signal,
          in_mid_net,
          num_mid_neuron,
          dim_in_signal + num_mid_neuron + 1);
      // 中間層の出力信号計算
      sigmoid_vec(in_mid_net,
          expand_mid_signal,
          num_mid_neuron);
      expand_mid_signal[num_mid_neuron] = 1;

      // 中間->出力層の出力信号和計算
      multiply_mat_vec(Wmid_out,
          expand_mid_signal,
          mid_out_net,
          dim_out_signal,
          num_mid_neuron + 1);
      // 出力層の出力信号計算
      sigmoid_vec(mid_out_net,
          out_signal,
          dim_out_signal);

      /*
      for (int i = 0; i < dim_out_signal; i++) {
        predict_signal[i] = expand_signal(out_signal[i],
            MATRIX_AT(sample_maxmin,2,i,0),
            MATRIX_AT(sample_maxmin,2,i,1));
      }
      */

      // ラベルとの二乗誤差
      for (int n = 0;n < dim_out_signal;n++) {
        squareError += powf((out_signal[n] - MATRIX_AT(norm_label,dim_out_signal,seq,n)),2);
      } 

      /* 学習ステップその2:逆誤差伝搬 */
      // 誤差信号の計算
      for (int n = 0; n < dim_out_signal; n++) {
        sigma[n] = (out_signal[n] - MATRIX_AT(norm_label,dim_out_signal,seq,n)) * out_signal[n] * (1 - out_signal[n]);
      }

      // 出力->中間層の係数の変更量計算
      for (int n = 0; n < dim_out_signal; n++) {
        for (int j = 0; j < num_mid_neuron + 1; j++) {
          MATRIX_AT(dWmid_out,num_mid_neuron,n,j) = sigma[n] * expand_mid_signal[j];
        }
      }

      // 中間->入力層の係数の変更量計算
      register float sum_sigma;
      for (int i = 0; i < num_mid_neuron; i++) {
        // 誤差信号を逆向きに伝播させる.
        sum_sigma = 0;
        for (int k = 0; k < dim_out_signal; k++) {
          sum_sigma += sigma[k] * MATRIX_AT(Wmid_out,num_mid_neuron + 1,k,i);
        }
        // 中間->入力層の係数の変更量計算
        for (int j = 0; j < col_in_mid; j++) {
          MATRIX_AT(dWin_mid,num_mid_neuron,j,i)
            = sum_sigma * expand_mid_signal[i] *
            (1 - expand_mid_signal[i]) *
            expand_in_signal[j];
        }
      }

      // 係数更新
      for (int i = 0; i < row_in_mid; i++) {
        for (int j = 0; j < col_in_mid; j++) {
          MATRIX_AT(Win_mid,col_in_mid,i,j) = 
            MATRIX_AT(Win_mid,col_in_mid,i,j) - 
            this->learnRate * MATRIX_AT(dWin_mid,col_in_mid,i,j) -
            this->alpha * MATRIX_AT(prevdWin_mid,col_in_mid,i,j);
        }
      }
      for (int i = 0; i < row_mid_out; i++) {
        for (int j = 0; j < col_mid_out; j++) {
          MATRIX_AT(Wmid_out,col_mid_out,i,j)= 
            MATRIX_AT(Wmid_out,col_mid_out,i,j) - 
            this->learnRate * MATRIX_AT(dWmid_out,col_mid_out,i,j) - 
            this->alpha * MATRIX_AT(prevdWmid_out,col_mid_out,i,j);
        }
      }

    }
    squareError = sqrt(squareError / len_seqence);

    /* 学習の終了判定 */
    // 終了フラグが立ち,かつ系列の最後に達していたら学習終了
    if ((fabsf(squareError - prevError) < epsilon) || (iteration > maxIteration)) {
      break;
    }

    // ループ回数のインクリメント
    iteration += 1;
    prevError = squareError;
  }

  return squareError;
}
