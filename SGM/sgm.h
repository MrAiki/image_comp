#ifndef SGM_H_INCLUDED
#define SGM_H_INCLUDED

#include <stdint.h>

/* SGMファイル */
struct SGMPicture {
  uint16_t  width;    /* 幅 */
  uint16_t  height;   /* 高さ */
  uint8_t*  gray;     /* グレースケール画素配列 */
};

/* アクセサ */
#define SGMPicture_GRAY(sgm, x, y) ((sgm)->gray[(x) + (y) * ((sgm)->width)])

/* SGMインスタンス作成 */
struct SGMPicture* SGMPicture_Create(uint16_t width, uint16_t height);

/* SGMファイル読み出し */
struct SGMPicture* SGMPicture_CreateFromFile(const char* filename);

/* SGMインスタンス破棄 */
void SGMPicture_Destroy(struct SGMPicture* sgm);

/* SGMファイル書き出し */
void SGMPicture_WriteToFile(const char* filename, const struct SGMPicture* sgm);

#endif /* SGM_H_INCLUDED */
