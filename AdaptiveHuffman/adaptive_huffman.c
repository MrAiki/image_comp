#include "adaptive_huffman.h"
#include <stdlib.h>
#include <string.h>

#define ADAPTIVE_HUFFMAN_SYMBOL_BITS        8       /* シンボルに割り当てるビット数 TODO:木側に持たせる */
#define ADAPTIVE_HUFFMAN_NUM_SYMBOLS \
  (1UL << ADAPTIVE_HUFFMAN_SYMBOL_BITS)       /* 通常のシンボル数 */
#define ADAPTIVE_HUFFMAN_END_OF_STREAM      (ADAPTIVE_HUFFMAN_NUM_SYMBOLS)        /* 終端文字 */
#define ADAPTIVE_HUFFMAN_ESCAPE             (ADAPTIVE_HUFFMAN_NUM_SYMBOLS+1)      /* エスケープ文字 */
#define ADAPTIVE_HUFFMAN_NUM_ALL_SYMBOLS    (ADAPTIVE_HUFFMAN_NUM_SYMBOLS+2)      /* 全シンボル数 */
#define ADAPTIVE_HUFFMAN_NUM_NODE_TABLE \
  ((ADAPTIVE_HUFFMAN_NUM_ALL_SYMBOLS * 2)-1)        /* ノード配列の数 */
#define ADAPTIVE_HUFFMAN_ROOT_NODE_INDEX    0       /* ルートノードのインデックス */
#define ADAPTIVE_HUFFMAN_MAX_WEIGHT         0x8000  /* ノードの最大重み */

#define ADAPTIVE_HUFFMAN_TRUE               1
#define ADAPTIVE_HUFFMAN_FALSE              0
#define ADAPTIVE_HUFFMAN_NODE_NOT_ALLOCATED (-1)    /* ノード未割り当て */

/* 適応的ハフマン木 */
struct AdaptiveHuffmanTree {
  int32_t leaf[ADAPTIVE_HUFFMAN_NUM_ALL_SYMBOLS]; /* 葉に対するノードインデックス */
  int32_t next_free_node;                         /* 次に割当予定のノード */
  struct AdaptiveHuffmanNode {
    uint32_t  weight;                             /* パターンの頻度 */
    int32_t   parent;                             /* 親ノード */
    int32_t   child_is_leaf;                      /* 子ノードは葉か？ */
    int32_t   child;                              /* 子ノードの一要素（もしくは実際の記号値） */
  } nodes[ADAPTIVE_HUFFMAN_NUM_NODE_TABLE];       /* ノード配列: 重みは降順、かつ、隣接する2つの要素は互い同一の親を持つ（兄弟特性を満たす） */
};

/* モデル（木）の更新 */
static void AdaptiveHuffman_UpdateModel(
    struct AdaptiveHuffmanTree* tree, uint32_t symbol);
/* 木の再構築 */
static void AdaptiveHuffman_RebuildTree(
    struct AdaptiveHuffmanTree* tree);
/* 新規ノードの追加 */
static void AdaptiveHuffman_AddNewNode(
    struct AdaptiveHuffmanTree* tree, uint32_t symbol);
/* ノードのスワップ */
static void AdaptiveHuffman_SwapNodes(
    struct AdaptiveHuffmanTree* tree, uint32_t node_i, uint32_t node_j);

/* 適応型ハフマン木の作成 */
struct AdaptiveHuffmanTree* AdaptiveHuffmanTree_Create(void)
{
  int32_t i_leaf;
  struct AdaptiveHuffmanTree* tree;

  /* 領域割当 */
  tree = (struct AdaptiveHuffmanTree*)malloc(sizeof(struct AdaptiveHuffmanTree));

  /* 葉の情報をクリア */
  for (i_leaf = 0; i_leaf < (int32_t)ADAPTIVE_HUFFMAN_NUM_ALL_SYMBOLS; i_leaf++) {
    tree->leaf[i_leaf] = ADAPTIVE_HUFFMAN_NODE_NOT_ALLOCATED;
  }

  /* ルートノードの作成 */
  tree->nodes[ADAPTIVE_HUFFMAN_ROOT_NODE_INDEX].child
    = ADAPTIVE_HUFFMAN_ROOT_NODE_INDEX + 1;
  tree->nodes[ADAPTIVE_HUFFMAN_ROOT_NODE_INDEX].child_is_leaf
    = ADAPTIVE_HUFFMAN_FALSE;
  tree->nodes[ADAPTIVE_HUFFMAN_ROOT_NODE_INDEX].weight
    = 2;
  tree->nodes[ADAPTIVE_HUFFMAN_ROOT_NODE_INDEX].parent
    = ADAPTIVE_HUFFMAN_NODE_NOT_ALLOCATED;

  /* EOSノードの作成 */
  tree->nodes[ADAPTIVE_HUFFMAN_ROOT_NODE_INDEX+1].child
    = ADAPTIVE_HUFFMAN_END_OF_STREAM;
  tree->nodes[ADAPTIVE_HUFFMAN_ROOT_NODE_INDEX+1].child_is_leaf
    = ADAPTIVE_HUFFMAN_TRUE;
  tree->nodes[ADAPTIVE_HUFFMAN_ROOT_NODE_INDEX+1].weight
    = 1;
  tree->nodes[ADAPTIVE_HUFFMAN_ROOT_NODE_INDEX+1].parent
    = ADAPTIVE_HUFFMAN_ROOT_NODE_INDEX;
  tree->leaf[ADAPTIVE_HUFFMAN_END_OF_STREAM]
    = ADAPTIVE_HUFFMAN_ROOT_NODE_INDEX+1;

  /* エスケープシンボルノードの作成 */
  tree->nodes[ADAPTIVE_HUFFMAN_ROOT_NODE_INDEX+2].child
    = ADAPTIVE_HUFFMAN_ESCAPE;
  tree->nodes[ADAPTIVE_HUFFMAN_ROOT_NODE_INDEX+2].child_is_leaf
    = ADAPTIVE_HUFFMAN_TRUE;
  tree->nodes[ADAPTIVE_HUFFMAN_ROOT_NODE_INDEX+2].weight
    = 1;
  tree->nodes[ADAPTIVE_HUFFMAN_ROOT_NODE_INDEX+2].parent
    = ADAPTIVE_HUFFMAN_ROOT_NODE_INDEX;
  tree->leaf[ADAPTIVE_HUFFMAN_ESCAPE]
    = ADAPTIVE_HUFFMAN_ROOT_NODE_INDEX+2;

  /* 次に割当予定のノードインデックス */
  tree->next_free_node = ADAPTIVE_HUFFMAN_ROOT_NODE_INDEX + 3;

  return tree;
}

/* 適応的ハフマン木の破棄 */
void AdaptiveHuffmanTree_Destroy(struct AdaptiveHuffmanTree* tree)
{
  if (tree != NULL) {
    free(tree);
  }
}

/* エンコード処理 */
AdaptiveHuffmanApiResult AdaptiveHuffman_EncodeSymbol(
    struct AdaptiveHuffmanTree* tree,
    struct BitStream* strm,
    uint32_t symbol)
{
  uint32_t  code;
  uint32_t  current_bit;
  int32_t   code_size;
  int32_t   current_node;

  /* 引数チェック */
  if (tree == NULL || strm == NULL
      || symbol >= ADAPTIVE_HUFFMAN_NUM_SYMBOLS) {
    return ADAPTIVE_HUFFMAN_APIRESULT_NG;
  }

  /* シンボルに対するノード取得 */
  current_node = tree->leaf[symbol];

  /* 未割り当て符号の場合はエスケープシンボルに割り当てる */
  if (current_node == ADAPTIVE_HUFFMAN_NODE_NOT_ALLOCATED) {
    current_node = tree->leaf[ADAPTIVE_HUFFMAN_ESCAPE];
  }

  /* ルートに達するまで木を上向きにたどる */
  code        = 0;
  current_bit = 1;
  code_size   = 0;
  while (current_node != ADAPTIVE_HUFFMAN_ROOT_NODE_INDEX) {
    /* ノードインデックスに応じて現在のビットを立てる */
    /* 偶数ノードならビットが立つ */
    if ((current_node & 1) == 0) {
      code |= current_bit;
    }
    current_bit <<= 1;
    code_size++;
    /* 次の親ノードへ */
    current_node = tree->nodes[current_node].parent;
  }

  /* コード出力 */
  BitStream_PutBits(strm, code_size, code);

  /* 未割り当てのノードの場合は
   * そのままのシンボルを続けて出力し、ノードを新規に登録する */
  if (tree->leaf[symbol] == ADAPTIVE_HUFFMAN_NODE_NOT_ALLOCATED) {
    BitStream_PutBits(strm, ADAPTIVE_HUFFMAN_SYMBOL_BITS, symbol);
    AdaptiveHuffman_AddNewNode(tree, symbol);
  }
  
  /* モデル更新 */
  AdaptiveHuffman_UpdateModel(tree, symbol);

  return ADAPTIVE_HUFFMAN_APIRESULT_OK;
}

/* エンコード処理 */
AdaptiveHuffmanApiResult AdaptiveHuffman_DecodeSymbol(
    struct AdaptiveHuffmanTree* tree,
    struct BitStream* strm,
    uint32_t* symbol)
{
  int32_t   current_node;
  uint32_t  tmp_symbol;
  uint64_t  bitsbuf;

  /* 引数チェック */
  if (tree == NULL || strm == NULL || symbol == NULL) {
    return ADAPTIVE_HUFFMAN_APIRESULT_NG;
  }

  /* ルートノードから下向きに葉ノードを探索する */
  current_node = ADAPTIVE_HUFFMAN_ROOT_NODE_INDEX;
  while (tree->nodes[current_node].child_is_leaf == ADAPTIVE_HUFFMAN_FALSE) {
    current_node = tree->nodes[current_node].child;
    /* 奇数番ノードの場合は1加える */
    current_node += BitStream_GetBit(strm);
  }

  /* シンボル取得 */
  tmp_symbol = tree->nodes[current_node].child;

  /* エスケープシンボルだった場合は続けて読むことで、
   * そのままのシンボルが得られる */
  if (tmp_symbol == ADAPTIVE_HUFFMAN_ESCAPE) {
    BitStream_GetBits(strm, ADAPTIVE_HUFFMAN_SYMBOL_BITS, &bitsbuf);
    tmp_symbol = (uint32_t)bitsbuf;
    AdaptiveHuffman_AddNewNode(tree, tmp_symbol);
  }

  /* モデル更新 */
  AdaptiveHuffman_UpdateModel(tree, tmp_symbol);

  *symbol = tmp_symbol;

  return ADAPTIVE_HUFFMAN_APIRESULT_OK;
}

/* モデル（木）の更新 */
static void AdaptiveHuffman_UpdateModel(
    struct AdaptiveHuffmanTree* tree, uint32_t symbol)
{
  int32_t current_node;
  int32_t new_node;

  /* ルートの重み（重みの総和）が最大を超えている場合は、
   * 木を再構築する */
  if (tree->nodes[ADAPTIVE_HUFFMAN_ROOT_NODE_INDEX].weight
      >= ADAPTIVE_HUFFMAN_MAX_WEIGHT) {
    AdaptiveHuffman_RebuildTree(tree);
  }

  /* 葉からルートノードに向かって重みの更新処理 */
  current_node = tree->leaf[symbol];
  while (current_node != ADAPTIVE_HUFFMAN_NODE_NOT_ALLOCATED) {
    /* 重み更新 */
    tree->nodes[current_node].weight++;
    /* current_nodeを置くべきノードを探索 */
    for (new_node = current_node;
         new_node > ADAPTIVE_HUFFMAN_ROOT_NODE_INDEX;
         new_node--) {
      /* より0に近いインデックスの重みが大きくなっている箇所が移動すべきノード */
      if (tree->nodes[new_node-1].weight 
          >= tree->nodes[current_node].weight) {
        break;
      }
    }
    /* 配置すべきノードと入れ替え */
    if (current_node != new_node) {
      AdaptiveHuffman_SwapNodes(tree, current_node, new_node);
      /* 入れ替え先のノードに移動 */
      current_node = new_node;
    }
    /* 親ノードへ移動 */
    current_node = tree->nodes[current_node].parent;
  }

}

/* 木の再構築 */
static void AdaptiveHuffman_RebuildTree(
    struct AdaptiveHuffmanTree* tree)
{
  int32_t node_i, node_j, node_k;
  uint32_t weight;

  /* next_free_node-1から0に向かって葉ノードを配置 */
  /* 同時に重みを2で割る */
  node_j = tree->next_free_node - 1;
  for (node_i = node_j;
       node_i >= ADAPTIVE_HUFFMAN_ROOT_NODE_INDEX;
       node_i--) {
    if (tree->nodes[node_i].child_is_leaf == ADAPTIVE_HUFFMAN_TRUE) {
      tree->nodes[node_j] = tree->nodes[node_i];
      /* 1を加えて重み1だったシンボルが重み0にならないように調整 */
      tree->nodes[node_j].weight = (tree->nodes[node_j].weight + 1) >> 1;
      node_j--;
    }
  }

  /* ADAPTIVE_HUFFMAN_ROOT_NODE_INDEX ... (中間ノード) ... node_j ... (葉) ... tree->next_free_node-1 */

  /* node_j は空きノードの先頭に移動している */
  /* 葉からルートノードに向かってノードを再配置 */
  for (node_i = tree->next_free_node - 2;
       node_j >= ADAPTIVE_HUFFMAN_ROOT_NODE_INDEX;
       node_i -= 2, node_j--) {

    /* node_j に重みを集約し、node_jを中間ノードにセット */
    tree->nodes[node_j].weight
      = tree->nodes[node_i].weight + tree->nodes[node_i+1].weight;
    weight = tree->nodes[node_j].weight;
    tree->nodes[node_j].child_is_leaf = ADAPTIVE_HUFFMAN_FALSE;

    /* weightを持つノードの位置探索 */
    /* weightよりも大きい重みを持つノードは前方に移動する */
    for (node_k = node_j + 1; weight < tree->nodes[node_k].weight; node_k++) { ; }
    node_k--;
    memmove(&tree->nodes[node_j], &tree->nodes[node_j+1],
        (node_k - node_j) * sizeof(struct AdaptiveHuffmanNode));
    tree->nodes[node_k].weight        = weight;
    tree->nodes[node_k].child         = node_i;
    tree->nodes[node_k].child_is_leaf = ADAPTIVE_HUFFMAN_FALSE;
  }

  /* ノードインデックスを振分ける */
  for (node_i = tree->next_free_node-1; 
       node_i >= ADAPTIVE_HUFFMAN_ROOT_NODE_INDEX; node_i--) {
    if (tree->nodes[node_i].child_is_leaf == ADAPTIVE_HUFFMAN_TRUE) {
      /* 子ノードが葉であれば子ノードに親ノードインデックスを登録 */
      node_k = tree->nodes[node_i].child;
      tree->leaf[node_k] = node_i;
    } else {
      /* 中間ノードであれば、子ノード2つに親ノードを登録 */
      node_k = tree->nodes[node_i].child;
      tree->nodes[node_k].parent
        = tree->nodes[node_k+1].parent = node_i;
    }
  }
}

/* 新規ノードの追加 */
static void AdaptiveHuffman_AddNewNode(
    struct AdaptiveHuffmanTree* tree, uint32_t symbol)
{
  int32_t lightest_node;    /* 最も重みの低いノード */
  int32_t new_node;         /* 新しく割り当てるノード */
  int32_t zero_weight_node; /* 新しいノードに付属する、重み0の葉ノード */

  /* それぞれのノードのインデックスを決定 */
  lightest_node     = tree->next_free_node - 1;
  new_node          = tree->next_free_node;
  zero_weight_node  = tree->next_free_node + 1;

  /* 重み0の葉ノードと中間ノード2つを新規に追加するため、2増やす */
  tree->next_free_node += 2;

  /* 新ノードを作成 */
  tree->nodes[new_node]                       = tree->nodes[lightest_node];
  tree->nodes[new_node].parent                = lightest_node;
  tree->leaf[tree->nodes[new_node].child]     = new_node;

  /* 新しいノードを子に持たせる */
  tree->nodes[lightest_node].child            = new_node;
  tree->nodes[lightest_node].child_is_leaf    = ADAPTIVE_HUFFMAN_FALSE;

  /* 重み0の葉ノードを新規作成 */
  tree->nodes[zero_weight_node].child         = symbol;
  tree->nodes[zero_weight_node].child_is_leaf = ADAPTIVE_HUFFMAN_TRUE;
  tree->nodes[zero_weight_node].weight        = 0;
  tree->nodes[zero_weight_node].parent        = lightest_node;
  tree->leaf[symbol]                          = zero_weight_node;
}

/* ノードのスワップ */
static void AdaptiveHuffman_SwapNodes(
    struct AdaptiveHuffmanTree* tree, uint32_t node_i, uint32_t node_j)
{
  struct AdaptiveHuffmanNode tmp;

  /* ノードインデックスのスワップ(接続情報は残す) */

  /* 子ノードの情報をスワップ */
  if (tree->nodes[node_i].child_is_leaf == ADAPTIVE_HUFFMAN_TRUE) {
    /* node_iが葉を保つ場合、葉にnode_jに設定 */
    tree->leaf[tree->nodes[node_i].child]             = node_j;
  } else {
    /* node_iの子ノードにnode_jの親ノードを設定 */
    tree->nodes[tree->nodes[node_i].child].parent     = node_j;
    tree->nodes[tree->nodes[node_i].child + 1].parent = node_j;
  }

  if (tree->nodes[node_j].child_is_leaf == ADAPTIVE_HUFFMAN_TRUE) {
    tree->leaf[tree->nodes[node_j].child]             = node_i;
  } else {
    tree->nodes[tree->nodes[node_j].child].parent     = node_i;
    tree->nodes[tree->nodes[node_j].child + 1].parent = node_i;
  }

  /* 親ノードの情報は残しつつも入れ替え */
  tmp = tree->nodes[node_i];
  tree->nodes[node_i]         = tree->nodes[node_j];
  tree->nodes[node_i].parent  = tmp.parent;
  tmp.parent                  = tree->nodes[node_j].parent;
  tree->nodes[node_j]         = tmp;
}
