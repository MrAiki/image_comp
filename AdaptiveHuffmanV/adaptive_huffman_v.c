#include "adaptive_huffman_v.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define ADAPTIVE_HUFFMAN_V_NUM_SYMBOLS(n_bits)      (1UL << (n_bits))                   /* 最大の記号語 */
#define ADAPTIVE_HUFFMAN_V_NUM_NODE_TABLE(n_bits)   (((1UL << (n_bits)) + 1) * 2 + 1)   /* ノード配列の数 */
#define ADAPTIVE_HUFFMAN_V_ROOT                     (-1)                                /* ルートノードのインデックス */
#define ADAPTIVE_HUFFMAN_V_NIL                      (-2)                                /* 無効値 */

#define ADAPTIVE_HUFFMAN_V_MAX_WEIGHT               0x8000          /* ノードの最大重み */

#define ADAPTIVE_HUFFMAN_V_TRUE                     1
#define ADAPTIVE_HUFFMAN_V_FALSE                    0

/* 適応的ハフマン木 */
struct AdaptiveHuffmanVTree {
  uint32_t    symbol_bits;            /* 1シンボルのビット幅 */
  int32_t*    order_list;
  int32_t     lti;                    /* LTI(Leaf to increment), 後で重みを増やす葉の番号 */
  int32_t     max_node;               /* 現在のノード数 */
  int32_t     now;                    /* 編集対象のノード */
  /* ノード */
  struct AdaptiveHuffmanVNode {
    int32_t   symbol;                 /* ノードが葉ならば、その記号語 */
    int32_t   parent;                 /* 親ノード */
    int32_t   left;                   /* 左子ノード */
    int32_t   right;                  /* 右子ノード */
    int32_t   order;                  /* ノードのオーダ（深さ） */
    int32_t   is_leaf;                /* ノードは葉か？(ADAPTIVE_HUFFMAN_V_FALSEでノード) */
    int32_t   weight;                 /* パターンの頻度 */
  } *nodes;
};

/* モデル（木）の更新 */
static void AdaptiveHuffmanV_UpdateModel(
    struct AdaptiveHuffmanVTree* tree, int32_t symbol);
/* 木の再構築 */
static void AdaptiveHuffmanV_RebuildTree(
    struct AdaptiveHuffmanVTree* tree);
/* ノードのスワップ */
static void AdaptiveHuffmanV_SwapNodes(
    struct AdaptiveHuffmanVTree* tree, int32_t node_i, int32_t node_j);
/* 現在注目しているノードにより、ノードのスライドと重みの増加を行う */
static void AdaptiveHuffmanV_SlideNodes(
    struct AdaptiveHuffmanVTree* tree);
/* 現在のノードが内部ノード、かつ重みが1大きい葉が存在する場合、
 * 重みが1だけ大きい葉のブロックの先頭ノードに現在のノードをスライドさせる
 * その後、現在のノードの重みを1増やしたあと、入れ替える前のノードの
 * 親pre_parentに移動する */
static void AdaptiveHuffmanV_SlideNodeToBlockHeadLeaf(
    struct AdaptiveHuffmanVTree* tree);
/* 現在のノードが葉であり、同一の重みを持つ内部ノードが
 * 存在する場合、同一の重みを持つノードブロックの先頭に
 * 葉をスライドさせる */
static void AdaptiveHuffmanV_SlideLeafToBlockHeadNode(
    struct AdaptiveHuffmanVTree* tree);
/* 現在の葉をその葉が含まれるブロックの先頭ノードと入れ替える */
static void AdaptiveHuffmanV_SwapLeafAndBlockHeadNode(
    struct AdaptiveHuffmanVTree* tree);
/* 新規シンボルsymbolが来たときのエスケープノードの更新 */
static void AdaptiveHuffmanV_UpdateEscapeNode(
    struct AdaptiveHuffmanVTree* tree, int32_t symbol);
/* 記号symbolに対応する葉の位置を探す */
static int32_t AdaptiveHuffmanV_SearchNode(
    struct AdaptiveHuffmanVTree* tree, int32_t symbol);

/* 適応型ハフマン木の作成 */
struct AdaptiveHuffmanVTree* AdaptiveHuffmanVTree_Create(uint32_t symbol_bits)
{
  int32_t i_node;
  struct AdaptiveHuffmanVTree* tree;

  /* 領域割当 */
  tree = (struct AdaptiveHuffmanVTree *)malloc(sizeof(struct AdaptiveHuffmanVTree));
  tree->symbol_bits = symbol_bits;

  tree->nodes = (struct AdaptiveHuffmanVNode *)malloc(sizeof(struct AdaptiveHuffmanVNode)
      * ADAPTIVE_HUFFMAN_V_NUM_NODE_TABLE(symbol_bits));
  tree->order_list = (int32_t *)malloc(sizeof(int32_t) * ADAPTIVE_HUFFMAN_V_NUM_NODE_TABLE(symbol_bits));

  /* ノードをクリア */
  for (i_node = 0; i_node < (int32_t)ADAPTIVE_HUFFMAN_V_NUM_NODE_TABLE(symbol_bits); i_node++) {
    tree->nodes[i_node].symbol
      = tree->nodes[i_node].parent
      = tree->nodes[i_node].left
      = tree->nodes[i_node].right
      = tree->nodes[i_node].order
      = tree->order_list[i_node]
      = ADAPTIVE_HUFFMAN_V_NIL;
    tree->nodes[i_node].is_leaf = ADAPTIVE_HUFFMAN_V_TRUE;
    tree->nodes[i_node].weight  = 0;
  }

  /* ルートノードを作成 */
  tree->nodes[0].parent = ADAPTIVE_HUFFMAN_V_ROOT;
  tree->nodes[0].order  = 0;
  tree->order_list[0]   = 0;
  tree->lti             = 0;
  tree->max_node        = 0;
  tree->now             = 0;

  /* EOFノードを木に登録 */
  AdaptiveHuffmanV_UpdateModel(tree, EOF);

  return tree;
}

/* 適応的ハフマン木の破棄 */
void AdaptiveHuffmanVTree_Destroy(struct AdaptiveHuffmanVTree* tree)
{
  if (tree != NULL) {
    if (tree->nodes != NULL) {
      free(tree->nodes);
    }
    if (tree->order_list != NULL) {
      free(tree->order_list);
    }
    free(tree);
  }
}

/* エンコード処理 */
AdaptiveHuffmanVApiResult AdaptiveHuffmanV_EncodeSymbol(
    struct AdaptiveHuffmanVTree* tree,
    struct BitStream* strm,
    int32_t symbol)
{
  int32_t pos, temp;
  uint32_t code, current_bit, code_size;
  struct AdaptiveHuffmanVNode* nodes;

  /* 引数チェック */
  if (tree == NULL || strm == NULL
      || symbol >= (int32_t)ADAPTIVE_HUFFMAN_V_NUM_SYMBOLS(tree->symbol_bits)) {
    return ADAPTIVE_HUFFMAN_V_APIRESULT_NG;
  }

  nodes = tree->nodes;
  pos   = AdaptiveHuffmanV_SearchNode(tree, symbol);
  temp  = pos;

  /* 木をルートまで辿りながら符号語を作成 */
  code        = 0;
  current_bit = 1;
  code_size   = 0;
  do {
    /* 右側ノードならばビットが立つ */
    if (nodes[nodes[pos].parent].right == pos) {
      code |= current_bit;
    }
    current_bit <<= 1;
    code_size++;
    pos = nodes[pos].parent;
  } while (nodes[pos].parent != ADAPTIVE_HUFFMAN_V_ROOT);

  /* コード出力 */
  BitStream_PutBits(strm, code_size, code);

  /* シンボルが未出現だった場合、そのままのシンボルを続けて出力 */
  if (temp == tree->max_node) {
    BitStream_PutBits(strm, tree->symbol_bits, symbol);
  }

  /* モデルの更新 */
  AdaptiveHuffmanV_UpdateModel(tree, symbol);

  return ADAPTIVE_HUFFMAN_V_APIRESULT_OK;
}

/* エンコード処理 */
AdaptiveHuffmanVApiResult AdaptiveHuffmanV_DecodeSymbol(
    struct AdaptiveHuffmanVTree* tree,
    struct BitStream* strm,
    int32_t* symbol)
{
  int32_t   pos;
  int32_t   tmp_symbol;
  uint64_t  bitsbuf;
  struct AdaptiveHuffmanVNode* nodes;

  /* 引数チェック */
  if (tree == NULL || strm == NULL || symbol == NULL) {
    return ADAPTIVE_HUFFMAN_V_APIRESULT_NG;
  }

  nodes = tree->nodes;

  /* 葉に達するまで下向きに木を辿る */
  pos = 0;
  while (nodes[pos].is_leaf == ADAPTIVE_HUFFMAN_V_FALSE) {
    if (BitStream_GetBit(strm) == 1) {
      pos = nodes[pos].right;
    } else {
      pos = nodes[pos].left;
    }
  }

  /* 未出現ノードであれば、次のシンボルを読み込む */
  if (pos == tree->max_node) {
    BitStream_GetBits(strm, tree->symbol_bits, &bitsbuf);
    tmp_symbol = (int32_t)bitsbuf;
  } else {
    tmp_symbol = nodes[pos].symbol;
  }

  /* モデル更新 */
  AdaptiveHuffmanV_UpdateModel(tree, tmp_symbol);

  *symbol = tmp_symbol;

  return ADAPTIVE_HUFFMAN_V_APIRESULT_OK;
}

/* モデル（木）の更新 */
static void AdaptiveHuffmanV_UpdateModel(
    struct AdaptiveHuffmanVTree* tree, int32_t symbol)
{
  /* ルートノードの重みが最大を超えていたら、木を再構築 */
  /* TODO: まだ再構築処理は未実装 */
  if (tree->nodes[0].weight > ADAPTIVE_HUFFMAN_V_MAX_WEIGHT) {
    AdaptiveHuffmanV_RebuildTree(tree);
  }

  tree->lti = 0;

  tree->now = AdaptiveHuffmanV_SearchNode(tree, symbol);
  if (tree->now == tree->max_node) {
    /* 新規シンボル: エスケープノードの更新 */
    AdaptiveHuffmanV_UpdateEscapeNode(tree, symbol);
  } else {
    /* ブロック先頭ノードと入れ替え */
    AdaptiveHuffmanV_SwapLeafAndBlockHeadNode(tree);
  }

  /* ルートノードに至るまでスライドと重みの増加 */
  while (tree->now != 0) {
    AdaptiveHuffmanV_SlideNodes(tree);
  }
  tree->nodes[0].weight++;

  /* LTIが指しているノードの増加処理 */
  if (tree->lti != 0) {
    tree->now = tree->lti;
    AdaptiveHuffmanV_SlideNodes(tree);
  }
}

/* 木の再構築 */
static void AdaptiveHuffmanV_RebuildTree(
    struct AdaptiveHuffmanVTree* tree)
{
  /* 未実装 */
  assert(tree != NULL);
}

/* ノードのスワップ */
static void AdaptiveHuffmanV_SwapNodes(
    struct AdaptiveHuffmanVTree* tree, int32_t node_i, int32_t node_j)
{
  int32_t temp;
  struct AdaptiveHuffmanVNode* nodes = tree->nodes;

  if (nodes[nodes[node_i].parent].left == node_i) {
    if (nodes[nodes[node_j].parent].left == node_j) {
      nodes[nodes[node_i].parent].left = node_j;
      nodes[nodes[node_j].parent].left = node_i;
    } else if (nodes[nodes[node_j].parent].right == node_j) {
      nodes[nodes[node_i].parent].left  = node_j;
      nodes[nodes[node_j].parent].right = node_i;
    }
  } else if (nodes[nodes[node_i].parent].right == node_i) {
    if (nodes[nodes[node_j].parent].left == node_j) {
      nodes[nodes[node_i].parent].right = node_j;
      nodes[nodes[node_j].parent].left  = node_i;
    } else if (nodes[nodes[node_j].parent].right == node_j) {
      nodes[nodes[node_i].parent].right = node_j;
      nodes[nodes[node_j].parent].right = node_i;
    }
  } else {
    assert(0);
  }

  /* order listの入れ替え */
  temp = tree->order_list[nodes[node_i].order];
  tree->order_list[nodes[node_i].order] = tree->order_list[nodes[node_j].order];
  tree->order_list[nodes[node_j].order] = temp;

  /* 親情報とオーダの入れ替え */
  temp = nodes[node_i].parent;
  nodes[node_i].parent = nodes[node_j].parent;
  nodes[node_j].parent = temp;
  temp = nodes[node_i].order;
  nodes[node_i].order = nodes[node_j].order;
  nodes[node_j].order = temp;
}

/* 記号symbolに対応する葉の位置を探す */
static int32_t AdaptiveHuffmanV_SearchNode(struct AdaptiveHuffmanVTree* tree, int32_t symbol)
{
  int32_t node;

  for (node = 0; node < tree->max_node; node++) {
    if (tree->nodes[node].symbol == symbol) {
      return node;
    }
  }

  return tree->max_node;
}

/* 
 * 新規シンボルsymbolが来たときのエスケープノードの更新
 *  1. 新たに2つの重み0の子を追加
 *  2. 左の葉を新たなエスケープノードとする
 *  3. 右の葉にsymbolを割り当てる
 *  4. ltiに右側の葉を割り当てる
 */
static void AdaptiveHuffmanV_UpdateEscapeNode(struct AdaptiveHuffmanVTree* tree, int32_t symbol)
{
  int32_t node, escape_node, symbol_node;
  struct AdaptiveHuffmanVNode* nodes = tree->nodes;
  const int32_t max_node = tree->max_node;

  escape_node = max_node + 2;
  symbol_node = max_node + 1;

  nodes[max_node].left     = escape_node;
  nodes[max_node].right    = symbol_node;
  nodes[max_node].is_leaf  = ADAPTIVE_HUFFMAN_V_FALSE; /* ノードに変える */
  nodes[escape_node].parent = max_node;
  nodes[escape_node].order  = 0;
  nodes[symbol_node].symbol = symbol;
  nodes[symbol_node].parent = max_node;
  nodes[symbol_node].order  = 1;

  /* ノードの番号を振り直す */
  for (node = 0; node < max_node + 1; node++) {
    nodes[node].order = nodes[node].order + 2;
  }
  for (node = max_node; node >= 0; node--) {
    tree->order_list[node + 2] = tree->order_list[node];
  }

  tree->order_list[0] = escape_node;
  tree->order_list[1] = symbol_node;

  tree->now = max_node;
  tree->lti = symbol_node;
  tree->max_node = max_node + 2;
}

/* 現在の葉をその葉が含まれるブロックの先頭ノードと入れ替える
 * もし、現在のノードがエスケープノードの兄弟であれば、LTIに現在の葉の
 * 配列での位置を入れ、現在の葉の親の接点に移動する */
static void AdaptiveHuffmanV_SwapLeafAndBlockHeadNode(struct AdaptiveHuffmanVTree* tree)
{
  int32_t next, temp;
  struct AdaptiveHuffmanVNode* nodes = tree->nodes;

  temp = tree->now;
  next = tree->order_list[nodes[tree->now].order + 1];

  /* ブロック先頭ノードの探索 */
  while (nodes[temp].weight == nodes[next].weight
          && nodes[temp].is_leaf == nodes[next].is_leaf) {
    next = tree->order_list[nodes[temp].order + 2];
    temp = tree->order_list[nodes[temp].order + 1];
  }

  /* ノード入れ替え */
  if (tree->now != temp) {
    AdaptiveHuffmanV_SwapNodes(tree, tree->now, temp);
  }

  if (nodes[tree->now].parent == nodes[tree->max_node].parent
      && nodes[tree->now].parent != ADAPTIVE_HUFFMAN_V_ROOT) {
    tree->lti = tree->now;
    tree->now = nodes[tree->now].parent;
  }
}

/* 現在のノードが葉であり、同一の重みを持つ内部ノードが
 * 存在する場合、同一の重みを持つノードブロックの先頭に
 * 葉をスライドさせる */
static void AdaptiveHuffmanV_SlideLeafToBlockHeadNode(struct AdaptiveHuffmanVTree* tree)
{
  struct AdaptiveHuffmanVNode* nodes = tree->nodes;
  int32_t next = tree->order_list[nodes[tree->now].order + 1];

  while (nodes[tree->now].weight == nodes[next].weight
          && nodes[next].is_leaf == ADAPTIVE_HUFFMAN_V_FALSE
          && nodes[next].parent  != ADAPTIVE_HUFFMAN_V_ROOT) {
    AdaptiveHuffmanV_SwapNodes(tree, tree->now, next);
    next = tree->order_list[nodes[tree->now].order + 1];
  }

  nodes[tree->now].weight++;
  tree->now = nodes[tree->now].parent;
}

/* 現在のノードが内部ノード、かつ重みが1大きい葉が存在する場合、
 * 重みが1だけ大きい葉のブロックの先頭ノードに現在のノードをスライドさせる
 * その後、現在のノードの重みを1増やしたあと、入れ替える前のノードの
 * 親pre_parentに移動する */
static void AdaptiveHuffmanV_SlideNodeToBlockHeadLeaf(struct AdaptiveHuffmanVTree* tree)
{
  struct AdaptiveHuffmanVNode* nodes = tree->nodes;
  int32_t next, weighter, pre_parent;

  next = tree->order_list[nodes[tree->now].order + 1];

  while (nodes[next].weight == nodes[tree->now].weight
          && nodes[next].is_leaf == ADAPTIVE_HUFFMAN_V_FALSE) {
    AdaptiveHuffmanV_SwapNodes(tree, tree->now, next);
    next = tree->order_list[nodes[tree->now].order + 1];
  }

  pre_parent = nodes[tree->now].parent;
  next       = tree->order_list[nodes[tree->now].order + 1];
  weighter   = nodes[tree->now].weight + 1;

  /* 重みが1だけ大きい葉のブロックへスライド */
  while (weighter == nodes[next].weight
          && nodes[next].is_leaf == ADAPTIVE_HUFFMAN_V_TRUE) {
    AdaptiveHuffmanV_SwapNodes(tree, tree->now, next);
    next = tree->order_list[nodes[tree->now].order + 1];
  }

  /* 重みの増加と親への移動 */
  nodes[tree->now].weight = nodes[tree->now].weight + 1;
  tree->now               = pre_parent;
}

/* 現在注目しているノードにより、ノードのスライドと重みの増加を行う */
static void AdaptiveHuffmanV_SlideNodes(struct AdaptiveHuffmanVTree* tree)
{
  if (tree->nodes[tree->now].is_leaf == ADAPTIVE_HUFFMAN_V_TRUE) {
    AdaptiveHuffmanV_SlideLeafToBlockHeadNode(tree);
  } else if (tree->nodes[tree->now].is_leaf == ADAPTIVE_HUFFMAN_V_FALSE) {
    AdaptiveHuffmanV_SlideNodeToBlockHeadLeaf(tree);
  } else {
    assert(0);
  }
}
