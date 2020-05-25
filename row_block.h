/*!
 *  Copyright (c) 2015 by Contributors
 * \file row_block.h
 * \brief additional data structure to support
 *        RowBlock data structure
 * \author Tianqi Chen
 */
#ifndef DMLC_DATA_ROW_BLOCK_H_
#define DMLC_DATA_ROW_BLOCK_H_

#include <dmlc/io.h>
#include <dmlc/logging.h>
#include <dmlc/data.h>
#include <cstring>
#include <vector>
#include <limits>
#include <algorithm>

namespace dmlc {
namespace data {
/*!
 * \brief dynamic data structure that holds
 *        a row block of unit data
 * \tparam IndexType the type of index we are using
 */
template<typename IndexType, typename DType = real_t>
struct UnitBlockContainer {
  /*! \brief array[size+1], row pointer to beginning of each rows */
  std::vector<size_t> offset;
  /*! \brief feature index */
  std::vector<IndexType> index;
  /*! \brief feature value */
  std::vector<DType> value;
  /*! \brief maximum value of index */
  IndexType max_index;
  // constructor
  UnitBlockContainer(void) {
    this->Clear();
  }
  /*! \brief clear the container */
  inline void Clear(void) {
    offset.clear(); offset.push_back(0);
    index.clear(); value.clear();
    max_index = 0;
  }
  /*! \return estimation of memory cost of this container */
  inline size_t MemCostBytes(void) const {
    return offset.size() * sizeof(size_t) +
        index.size() * sizeof(IndexType) +
        value.size() * sizeof(DType);
  }
  /*! \brief convert to a row block */
  inline UnitBlock<IndexType, DType> GetBlock(void) const;
  /*!
   * \brief push the unit row into container
   * \param row the row to push back
   * \tparam I the index type of the row
   */
  template<typename I, typename D>
  inline void Push(UnitData<I, D> row) {
    for (size_t i = 0; i < row.length; ++i) {
      CHECK_LE(row.index[i], std::numeric_limits<IndexType>::max())
          << "index exceed numeric bound of current type";
      IndexType findex = static_cast<IndexType>(row.index[i]);
      index.push_back(findex);
      max_index = std::max(max_index, findex);
    }
    if (row.value != NULL) {
      for (size_t i = 0; i < row.length; ++i) {
        value.push_back(row.value[i]);
      }
    }
    offset.push_back(index.size());
  }
  /*!
   * \brief push the row unit block into container
   * \param row the row to push back
   * \tparam I the index type of the row
   */
  template<typename I, typename D>
  inline void Push(UnitBlock<I, D> batch, size_t size) {
    CHECK_EQ(batch.size, size) << "UnitBlock size is not equal to size: "
                               << batch.size << " vs " << size;
    size_t ndata = batch.offset[batch.size] - batch.offset[0];
    index.resize(index.size() + ndata);
    IndexType *ihead = BeginPtr(index) + offset.back();
    for (size_t i = 0; i < ndata; ++i) {
      CHECK_LE(batch.index[i], std::numeric_limits<IndexType>::max())
          << "index  exceed numeric bound of current type";
      IndexType findex = static_cast<IndexType>(batch.index[i]);
      ihead[i] = findex;
      max_index = std::max(max_index, findex);
    }
    if (batch.value != NULL) {
      value.resize(value.size() + ndata);
      std::memcpy(BeginPtr(value) + value.size() - ndata, batch.value,
                  ndata * sizeof(DType));
    }
    size_t shift = offset[size];
    offset.resize(offset.size() + batch.size);
    size_t *ohead = BeginPtr(offset) + size + 1;
    for (size_t i = 0; i < batch.size; ++i) {
      ohead[i] = shift + batch.offset[i + 1] - batch.offset[0];
    }
  }
};

template<typename IndexType, typename DType>
inline UnitBlock<IndexType, DType>
UnitBlockContainer<IndexType, DType>::GetBlock(void) const {
  // consistency check
  CHECK_EQ(offset.back(), index.size());
  CHECK(offset.back() == value.size() || value.size() == 0);
  UnitBlock<IndexType> data;
  data.size = offset.size() - 1;
  data.offset = BeginPtr(offset);
  data.index = BeginPtr(index);
  data.value = BeginPtr(value);
  return data;
}
/*!
 * \brief dynamic data structure that holds
 *        a row block of data
 * \tparam IndexType the type of index we are using
 */
template<typename IndexType, typename DType = real_t>
struct RowBlockContainer {
  /*! \brief array[size+1], row pointer to beginning of each rows */
  std::vector<size_t> offset;
  /*! \brief label width of each instance */
  size_t label_width = 1;
  /*! \brief array[size] label of each instance */
  std::vector<DType> label;
  /*! \brief array[size] weight of each instance */
  std::vector<real_t> weight;
  /*! \brief array[size] session-id of each instance */
  std::vector<uint64_t> qid;
  /*! \brief field index */
  std::vector<IndexType> field;
  /*! \brief feature index */
  std::vector<IndexType> index;
  /*! \brief feature value */
  std::vector<DType> value;
  /*! \brief maximum value of field */
  IndexType max_field;
  /*! \brief maximum value of index */
  IndexType max_index;
  /*! \brief extra data */
  //TODO
  //UnitBlockContainer<IndexType> dense;
  //UnitBlockContainer<IndexType, IndexType> cate;
  std::vector<UnitBlockContainer<IndexType> > extra;
  // constructor
  RowBlockContainer(void) {
    this->Clear();
  }
  /*! \brief convert to a row block */
  inline RowBlock<IndexType, DType> GetBlock(void) const;
  /*!
   * \brief write the row block to a binary stream
   * \param fo output stream
   */
  inline void Save(Stream *fo) const;
  /*!
   * \brief load row block from a binary stream
   * \param fi output stream
   * \return false if at end of file
   */
  inline bool Load(Stream *fi);
  /*! \brief clear the container */
  inline void Clear(void) {
    offset.clear(); offset.push_back(0);
    label.clear(); field.clear(); index.clear(); value.clear(); weight.clear(); qid.clear();
    max_field = 0;
    max_index = 0;
    for (auto it = extra.begin(); it != extra.end(); it++)
      it->Clear();
  }
  /*! \brief size of the data */
  inline size_t Size(void) const {
    return offset.size() - 1;
  }
  /*! \return estimation of memory cost of this container */
  inline size_t MemCostBytes(void) const {
    size_t total = 0;
    for (auto it = extra.begin(); it != extra.end(); it++)
        total += it->MemCostBytes();
    return total + offset.size() * sizeof(size_t) +
        label.size() * sizeof(real_t) +
        weight.size() * sizeof(real_t) +
        qid.size() * sizeof(size_t) +
        field.size() * sizeof(IndexType) +
        index.size() * sizeof(IndexType) +
        value.size() * sizeof(DType);
  }
  /*!
   * \brief push the row into container
   * \param row the row to push back
   * \tparam I the index type of the row
   */
  template<typename I>
  inline void Push(Row<I, DType> row) {
    for (size_t i = 0; i < row.label_width; ++i)
      label.push_back(row.label[i]);
    weight.push_back(row.get_weight());
    qid.push_back(row.get_qid());
    if (row.field != NULL) {
      for (size_t i = 0; i < row.length; ++i) {
        CHECK_LE(row.field[i], std::numeric_limits<IndexType>::max())
            << "field exceed numeric bound of current type";
        IndexType field_id = static_cast<IndexType>(row.field[i]);
        field.push_back(field_id);
        max_field = std::max(max_field, field_id);
    }
    }
    for (size_t i = 0; i < row.length; ++i) {
      CHECK_LE(row.index[i], std::numeric_limits<IndexType>::max())
          << "index exceed numeric bound of current type";
      IndexType findex = static_cast<IndexType>(row.index[i]);
      index.push_back(findex);
      max_index = std::max(max_index, findex);
    }
    if (row.value != NULL) {
      for (size_t i = 0; i < row.length; ++i) {
        value.push_back(row.value[i]);
      }
    }
    for (size_t i = 0; i < row.extra.size(); ++i) {
      extra[i].Push(row.extra[i]);
    }
    offset.push_back(index.size());
  }
  /*!
   * \brief push the row block into container
   * \param row the row to push back
   * \tparam I the index type of the row
   */
  template<typename I>
  inline void Push(RowBlock<I, DType> batch) {
    size_t size = label.size();
    label.resize(label.size() + (batch.size * label_width));
    std::memcpy(BeginPtr(label) + size, batch.label,
                (batch.size* label_width) * sizeof(real_t));
    if (batch.weight != NULL) {
      weight.insert(weight.end(), batch.weight, batch.weight + batch.size);
    }
    if (batch.qid != NULL) {
      qid.insert(qid.end(), batch.qid, batch.qid + batch.size);
    }
    size_t ndata = batch.offset[batch.size] - batch.offset[0];
    if (batch.field != NULL) {
      field.resize(field.size() + ndata);
      IndexType *fhead = BeginPtr(field) + offset.back();
      for (size_t i = 0; i < ndata; ++i) {
        CHECK_LE(batch.field[i], std::numeric_limits<IndexType>::max())
            << "field  exceed numeric bound of current type";
        IndexType field_id = static_cast<IndexType>(batch.field[i]);
        fhead[i] = field_id;
        max_field = std::max(max_field, field_id);
      }
    }
    index.resize(index.size() + ndata);
    IndexType *ihead = BeginPtr(index) + offset.back();
    for (size_t i = 0; i < ndata; ++i) {
      CHECK_LE(batch.index[i], std::numeric_limits<IndexType>::max())
          << "index  exceed numeric bound of current type";
      IndexType findex = static_cast<IndexType>(batch.index[i]);
      ihead[i] = findex;
      max_index = std::max(max_index, findex);
    }
    if (batch.value != NULL) {
      value.resize(value.size() + ndata);
      std::memcpy(BeginPtr(value) + value.size() - ndata, batch.value,
                  ndata * sizeof(DType));
    }
    size_t shift = offset[size];
    offset.resize(offset.size() + batch.size);
    size_t *ohead = BeginPtr(offset) + size + 1;
    for (size_t i = 0; i < batch.size; ++i) {
      ohead[i] = shift + batch.offset[i + 1] - batch.offset[0];
    }
    for (size_t i = 0; i < batch.extra.size(); ++i) {
      extra[i].Push(batch.extra[i], size);
    }
  }
};

template<typename IndexType, typename DType>
inline RowBlock<IndexType, DType>
RowBlockContainer<IndexType, DType>::GetBlock(void) const {
  // consistency check
  if (label.size()) {
    CHECK_EQ((label.size() / label_width) + 1, offset.size());
  }
  CHECK_EQ(offset.back(), index.size());
  CHECK(offset.back() == value.size() || value.size() == 0);
  RowBlock<IndexType, DType> data;
  data.label_width = label_width;
  data.size = offset.size() - 1;
  data.offset = BeginPtr(offset);
  data.label = BeginPtr(label);
  data.weight = BeginPtr(weight);
  data.qid = BeginPtr(qid);
  data.field = BeginPtr(field);
  data.index = BeginPtr(index);
  data.value = BeginPtr(value);
  data.extra.resize(extra.size());
  for (int i = 0; i < extra.size(); ++i)
    data.extra[i] = extra[i].GetBlock();
  return data;
}
template<typename IndexType, typename DType>
inline void
RowBlockContainer<IndexType, DType>::Save(Stream *fo) const {
  fo->Write(offset);
  fo->Write(label);
  fo->Write(weight);
  fo->Write(qid);
  fo->Write(field);
  fo->Write(index);
  fo->Write(value);
  fo->Write(&max_field, sizeof(IndexType));
  fo->Write(&max_index, sizeof(IndexType));
}
template<typename IndexType, typename DType>
inline bool
RowBlockContainer<IndexType, DType>::Load(Stream *fi) {
  if (!fi->Read(&offset)) return false;
  CHECK(fi->Read(&label)) << "Bad RowBlock format";
  CHECK(fi->Read(&weight)) << "Bad RowBlock format";
  CHECK(fi->Read(&qid)) << "Bad RowBlock format";
  CHECK(fi->Read(&field)) << "Bad RowBlock format";
  CHECK(fi->Read(&index)) << "Bad RowBlock format";
  CHECK(fi->Read(&value)) << "Bad RowBlock format";
  CHECK(fi->Read(&max_field, sizeof(IndexType))) << "Bad RowBlock format";
  CHECK(fi->Read(&max_index, sizeof(IndexType))) << "Bad RowBlock format";
  return true;
}
}  // namespace data
}  // namespace dmlc
#endif  // DMLC_DATA_ROW_BLOCK_H_
