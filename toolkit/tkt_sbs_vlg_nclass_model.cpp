/*
 * vulgaris
 * (C) 2018 - giuseppe.baccini@live.com
 *
 */

#include "tkt_sbs_vlg_nclass_model.h"

VLG_CLASS_ROW_IDX_PAIR::VLG_CLASS_ROW_IDX_PAIR(int rowidx,
                                               vlg::nclass *obj) :
    rowidx_(rowidx),
    obj_(obj)
{}

VLG_CLASS_ROW_IDX_PAIR &VLG_CLASS_ROW_IDX_PAIR::operator =(const VLG_CLASS_ROW_IDX_PAIR &src)
{
    rowidx_ = src.rowidx_;
    obj_ = src.obj_;
    return *this;
}

//-----------------------------------------------------------------------------
// subscription support structs
//-----------------------------------------------------------------------------
sbs_col_data_timer::sbs_col_data_timer(VLG_SBS_COL_DATA_ENTRY &parent) :
    parent_(parent)
{
    connect(this, SIGNAL(timeout()), this, SLOT(OnTimeout()));
}

void sbs_col_data_timer::OnTimeout()
{
    emit SignalCellResetColor(parent_);
}

VLG_SBS_COL_DATA_ENTRY::VLG_SBS_COL_DATA_ENTRY() :
    col_act_(BlazeSbsEntryActUndef),
    color_timer_(new sbs_col_data_timer(*this)),
    row_idx_(-1),
    col_idx_(-1)
{}

VLG_SBS_COL_DATA_ENTRY::~VLG_SBS_COL_DATA_ENTRY()
{
    delete color_timer_;
}

VLG_SBS_DATA_ENTRY::VLG_SBS_DATA_ENTRY(std::shared_ptr<vlg::nclass> &entry,
                                       int row_idx,
                                       int col_num) :
    entry_(entry),
    fld_act_(col_num)
{
    for(int i = 0; i<fld_act_.size(); i++) {
        fld_act_[i].row_idx_ = row_idx;
        fld_act_[i].col_idx_ = i;
    }
}

void vlg_toolkit_sbs_vlg_class_model::OnCellResetColor(VLG_SBS_COL_DATA_ENTRY
                                                       &scde)
{
    const QModelIndex &chng_index = createIndex(scde.row_idx_, scde.col_idx_);
    scde.color_timer_->stop();
    scde.col_act_ = BlazeSbsEntryActUndef;
    emit(dataChanged(chng_index, chng_index));
}

//------------------------------------------------------------------------------
// GENERATE REP SECTION BGN
//------------------------------------------------------------------------------

ENM_GEN_SBS_REP_REC_UD::ENM_GEN_SBS_REP_REC_UD(vlg_toolkit_sbs_vlg_class_model &mdl,
                                               const vlg::nentity_manager &bem,
                                               std::string *prfx,
                                               bool array_fld,
                                               unsigned int fld_idx) :
    mdl_(mdl),
    bem_(bem),
    prfx_(prfx),
    array_fld_(array_fld),
    fld_idx_(fld_idx)
{}

ENM_GEN_SBS_REP_REC_UD::~ENM_GEN_SBS_REP_REC_UD()
{}

bool enum_generate_sbs_model_rep(const vlg::member_desc &mmbrd,
                                 void *ud)
{
    ENM_GEN_SBS_REP_REC_UD *rud = (ENM_GEN_SBS_REP_REC_UD *)ud;
    std::string hdr_col_nm;
    hdr_col_nm.assign("");
    std::string idx_prfx;
    idx_prfx.assign(*(rud->prfx_));
    char idx_b[GEN_HDR_FIDX_BUFF] = {0};
    int sprintf_dbg = 0;

    if(rud->array_fld_) {
        sprintf_dbg = sprintf(idx_b, "%s%d", idx_prfx.length() ? "_" : "", rud->fld_idx_);
        idx_prfx.append(idx_b);
    }

    if(mmbrd.get_field_vlg_type() == vlg::Type_ENTITY) {
        if(mmbrd.get_field_nentity_type() == vlg::NEntityType_NENUM) {
            //treat enum as number
            if(mmbrd.get_field_nmemb() > 1) {
                for(unsigned int i = 0; i<mmbrd.get_field_nmemb(); i++) {
                    if(rud->prfx_->length()) {
                        hdr_col_nm.append(idx_prfx);
                        hdr_col_nm.append("_");
                    }
                    sprintf_dbg = sprintf(idx_b, "_%d", i);
                    hdr_col_nm.append(mmbrd.get_member_name());
                    hdr_col_nm.append(idx_b);
                    rud->mdl_.AppendHdrColumn(hdr_col_nm.c_str());
                    rud->mdl_.IncrColnum();
                    hdr_col_nm.assign("");
                }
            } else {
                if(rud->prfx_->length()) {
                    hdr_col_nm.append(idx_prfx);
                    hdr_col_nm.append("_");
                }
                hdr_col_nm.append(mmbrd.get_member_name());
                rud->mdl_.AppendHdrColumn(hdr_col_nm.c_str());
                rud->mdl_.IncrColnum();
            }
        } else {
            //class, struct is a recursive step.
            ENM_GEN_SBS_REP_REC_UD rrud = *rud;
            std::string rprfx;
            rprfx.assign(idx_prfx);
            if(rprfx.length()) {
                rprfx.append("_");
            }
            rprfx.append(mmbrd.get_member_name());
            rrud.prfx_ = &rprfx;
            const vlg::nentity_desc *edsc = rud->bem_.get_nentity_descriptor(mmbrd.get_field_user_type());
            if(edsc) {
                if(mmbrd.get_field_nmemb() > 1) {
                    rrud.array_fld_ = true;
                    for(unsigned int i = 0; i<mmbrd.get_field_nmemb(); i++) {
                        rrud.fld_idx_ = i;
                        edsc->enum_member_descriptors(enum_generate_sbs_model_rep, &rrud);
                    }
                } else {
                    edsc->enum_member_descriptors(enum_generate_sbs_model_rep, &rrud);
                }
            } else {
                //ERROR
            }
        }
    } else {
        //primitive type
        if(mmbrd.get_field_vlg_type() == vlg::Type_ASCII || mmbrd.get_field_vlg_type() == vlg::Type_BYTE) {
            if(rud->prfx_->length()) {
                hdr_col_nm.append(idx_prfx);
                hdr_col_nm.append("_");
            }
            hdr_col_nm.append(mmbrd.get_member_name());
            rud->mdl_.AppendHdrColumn(hdr_col_nm.c_str());
            rud->mdl_.IncrColnum();
        } else if(mmbrd.get_field_nmemb() > 1) {
            for(unsigned int i = 0; i<mmbrd.get_field_nmemb(); i++) {
                if(rud->prfx_->length()) {
                    hdr_col_nm.append(idx_prfx);
                    hdr_col_nm.append("_");
                }
                sprintf_dbg = sprintf(idx_b, "_%d", i);
                hdr_col_nm.append(mmbrd.get_member_name());
                hdr_col_nm.append(idx_b);
                rud->mdl_.AppendHdrColumn(hdr_col_nm.c_str());
                rud->mdl_.IncrColnum();
                hdr_col_nm.assign("");
            }
        } else {
            if(rud->prfx_->length()) {
                hdr_col_nm.append(idx_prfx);
                hdr_col_nm.append("_");
            }
            hdr_col_nm.append(mmbrd.get_member_name());
            rud->mdl_.AppendHdrColumn(hdr_col_nm.c_str());
            rud->mdl_.IncrColnum();
        }
    }
    return true;
}

//------------------------------------------------------------------------------
// GENERATE REP SECTION END
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// UPDATE NCLASS ROW SECTION BGN
//------------------------------------------------------------------------------

ENM_UPD_CLS_ROW_REC_UD::ENM_UPD_CLS_ROW_REC_UD(vlg_toolkit_sbs_vlg_class_model
                                               &mdl,
                                               const vlg::nentity_manager &bem,
                                               const char *obj_ptr,
                                               const char *obj_ptr_prev,
                                               int rowidx,
                                               bool array_fld,
                                               unsigned int fld_idx) :
    mdl_(mdl),
    bem_(bem),
    obj_ptr_(obj_ptr),
    obj_ptr_prev_(obj_ptr_prev),
    rowidx_(rowidx),
    curcolidx_(0),
    array_fld_(array_fld),
    fld_idx_(fld_idx)
{}

ENM_UPD_CLS_ROW_REC_UD::~ENM_UPD_CLS_ROW_REC_UD()
{}

bool enum_update_class_row(const vlg::member_desc &mmbrd,
                           void *ud)
{
    ENM_UPD_CLS_ROW_REC_UD *rud = (ENM_UPD_CLS_ROW_REC_UD *)ud;
    const char *obj_f_ptr_new = NULL, *obj_f_ptr_prev = NULL;
    QString val;
    QModelIndex qindex;

    if(mmbrd.get_field_vlg_type() == vlg::Type_ENTITY) {
        if(mmbrd.get_field_nentity_type() == vlg::NEntityType_NENUM) {
            //treat enum as number
            if(mmbrd.get_field_nmemb() > 1) {
                for(unsigned int i = 0; i<mmbrd.get_field_nmemb(); i++) {
                    //new value ptr
                    obj_f_ptr_new = rud->obj_ptr_ +
                                    mmbrd.get_field_offset() +
                                    mmbrd.get_field_type_size()*i;
                    //old value ptr
                    obj_f_ptr_prev = rud->obj_ptr_prev_ +
                                     mmbrd.get_field_offset() +
                                     mmbrd.get_field_type_size()*i;

                    if(memcmp(obj_f_ptr_new, obj_f_ptr_prev, mmbrd.get_field_type_size())) {
                        FillQstring_FldValue(obj_f_ptr_new, mmbrd, val);
                        qindex = rud->mdl_.index(rud->rowidx_, rud->curcolidx_, QModelIndex());
                        rud->mdl_.setData(qindex, val, Qt::EditRole);
                    }
                    rud->curcolidx_++;
                }
            } else {
                //new value ptr
                obj_f_ptr_new = rud->obj_ptr_ + mmbrd.get_field_offset();
                //old value ptr
                obj_f_ptr_prev = rud->obj_ptr_prev_ + mmbrd.get_field_offset();
                if(memcmp(obj_f_ptr_new, obj_f_ptr_prev, mmbrd.get_field_type_size())) {
                    FillQstring_FldValue(obj_f_ptr_new, mmbrd, val);
                    qindex = rud->mdl_.index(rud->rowidx_, rud->curcolidx_, QModelIndex());
                    rud->mdl_.setData(qindex, val, Qt::EditRole);
                }
                rud->curcolidx_++;
            }
        } else {
            //class, struct is a recursive step.
            ENM_UPD_CLS_ROW_REC_UD rrud = *rud;
            const vlg::nentity_desc *edsc = rud->bem_.get_nentity_descriptor(mmbrd.get_field_user_type());
            if(edsc) {
                if(mmbrd.get_field_nmemb() > 1) {
                    rrud.array_fld_ = true;
                    for(unsigned int i = 0; i<mmbrd.get_field_nmemb(); i++) {
                        rrud.fld_idx_ = i;
                        edsc->enum_member_descriptors(enum_update_class_row, &rrud);
                    }
                } else {
                    edsc->enum_member_descriptors(enum_update_class_row, &rrud);
                }
            } else {
                //ERROR
            }
            rud->curcolidx_ = rrud.curcolidx_;
        }
    } else {
        //primitive type
        if(mmbrd.get_field_vlg_type() == vlg::Type_ASCII || mmbrd.get_field_vlg_type() == vlg::Type_BYTE) {
            //new value ptr
            obj_f_ptr_new = rud->obj_ptr_ + mmbrd.get_field_offset();
            //old value ptr
            obj_f_ptr_prev = rud->obj_ptr_prev_ + mmbrd.get_field_offset();
            if(memcmp(obj_f_ptr_new, obj_f_ptr_prev, mmbrd.get_field_type_size()*mmbrd.get_field_nmemb())) {
                if(mmbrd.get_field_vlg_type() == vlg::Type_ASCII) {
                    val = QString::fromLatin1(obj_f_ptr_new, (int)mmbrd.get_field_nmemb());
                } else {
                    val = QString::fromUtf8(obj_f_ptr_new, (int)mmbrd.get_field_nmemb());
                }
                qindex = rud->mdl_.index(rud->rowidx_, rud->curcolidx_, QModelIndex());
                rud->mdl_.setData(qindex, val, Qt::EditRole);
            }
            rud->curcolidx_++;
        } else if(mmbrd.get_field_nmemb() > 1) {
            for(unsigned int i = 0; i<mmbrd.get_field_nmemb(); i++) {
                //new value ptr
                obj_f_ptr_new = rud->obj_ptr_ + mmbrd.get_field_offset() + mmbrd.get_field_type_size()*i;
                //old value ptr
                obj_f_ptr_prev = rud->obj_ptr_prev_ + mmbrd.get_field_offset() + mmbrd.get_field_type_size()*i;
                if(memcmp(obj_f_ptr_new, obj_f_ptr_prev, mmbrd.get_field_type_size())) {
                    FillQstring_FldValue(obj_f_ptr_new, mmbrd, val);
                    qindex = rud->mdl_.index(rud->rowidx_, rud->curcolidx_, QModelIndex());
                    rud->mdl_.setData(qindex, val, Qt::EditRole);
                }
                rud->curcolidx_++;
            }
        } else {
            //value
            obj_f_ptr_new = rud->obj_ptr_ + mmbrd.get_field_offset();
            //old value ptr
            obj_f_ptr_prev = rud->obj_ptr_prev_ + mmbrd.get_field_offset();
            if(memcmp(obj_f_ptr_new, obj_f_ptr_prev, mmbrd.get_field_type_size())) {
                FillQstring_FldValue(obj_f_ptr_new, mmbrd, val);
                qindex = rud->mdl_.index(rud->rowidx_, rud->curcolidx_, QModelIndex());
                rud->mdl_.setData(qindex, val, Qt::EditRole);
            }
            rud->curcolidx_++;
        }
    }
    return true;
}

//------------------------------------------------------------------------------
// vlg_toolkit_sbs_vlg_class_model
//------------------------------------------------------------------------------


vlg_toolkit_sbs_vlg_class_model::vlg_toolkit_sbs_vlg_class_model(
    const vlg::nentity_desc &edesc,
    const vlg::nentity_manager &bem,
    QObject *parent) :
    edesc_(edesc),
    bem_(bem),
    colnum_(0),
    QAbstractTableModel(parent)
{
    GenerateModelRep();
}

vlg_toolkit_sbs_vlg_class_model::~vlg_toolkit_sbs_vlg_class_model()
{

}

int vlg_toolkit_sbs_vlg_class_model::columnCount(const QModelIndex &parent)
const
{
    return colnum_;
}

int vlg_toolkit_sbs_vlg_class_model::rowCount(const QModelIndex &parent) const
{
    return data_.size();
}

QModelIndex vlg_toolkit_sbs_vlg_class_model::index(int row,
                                                   int column,
                                                   const QModelIndex &parent) const
{
    if(row < 0 || column < 0 || column >= colnum_ || row >= data_.size()) {
        return QModelIndex();
    }
    return createIndex(row, column, data_[row].entry_.get());
}

QVariant vlg_toolkit_sbs_vlg_class_model::headerData(int section,
                                                     Qt::Orientation orientation,
                                                     int role) const
{
    if(role != Qt::DisplayRole) {
        return QVariant();
    }
    if(orientation == Qt::Horizontal) {
        return  QString("[%1][%2]").arg(section).arg(hdr_colidx_colname_[section]);
    } else {
        return QVariant();
    }
    return QVariant();
}

QVariant vlg_toolkit_sbs_vlg_class_model::data(const QModelIndex &index,
                                               int role) const
{
    if(!index.isValid()) {
        return QVariant();
    }
    if(index.row() >= data_.size() || index.row() < 0) {
        return QVariant();
    }
    if(index.column() >= colnum_ || index.column() < 0) {
        return QVariant();
    }
    if(role == Qt::DisplayRole) {
        vlg::nclass *obj = data_[index.row()].entry_.get();
        if(obj) {
            const vlg::member_desc *obj_fld_mdesc = NULL;
            const char *obj_fld_ptr = NULL;
            if((obj_fld_ptr = obj->get_field_address_by_column_number(index.column(), bem_, &obj_fld_mdesc))) {
                QString out;
                if((obj_fld_mdesc->get_field_vlg_type() == vlg::Type_ASCII) && obj_fld_mdesc->get_field_nmemb() > 1) {
                    out = QString::fromLatin1(obj_fld_ptr, (int)obj_fld_mdesc->get_field_nmemb());
                } else if((obj_fld_mdesc->get_field_vlg_type() == vlg::Type_BYTE) && obj_fld_mdesc->get_field_nmemb() > 1) {
                    out = QString::fromUtf8(obj_fld_ptr, (int)obj_fld_mdesc->get_field_nmemb());
                } else {
                    FillQstring_FldValue(obj_fld_ptr, *obj_fld_mdesc, out);
                }
                return out;
            } else {
                return tr("error");
            }
        } else {
            return QVariant();
        }
    }
    if(role == Qt::BackgroundRole) {
        BlazeSbsEntryAct sea = data_[index.row()].fld_act_[index.column()].col_act_;
        QBrush bckgrColor(Qt::white);
        if(sea == BlazeSbsEntryActRwt) {
            bckgrColor = Qt::yellow;
        }
        return bckgrColor;
    }
    return QVariant();
}

bool vlg_toolkit_sbs_vlg_class_model::setData(const QModelIndex &index,
                                              const QVariant &value,
                                              int role)
{
    if(!index.isValid()) {
        return false;
    }
    if(index.row() >= data_.size() || index.row() < 0) {
        return false;
    }
    if(index.column() >= colnum_ || index.column() < 0) {
        return false;
    }
    qDebug() << tr("editing:[%1,%2]:%3").arg(index.row()).arg(index.column()).arg(
                 value.toString());
    if(role == Qt::EditRole) {
        const QModelIndex &chng_index = createIndex(index.row(), index.column());
        data_[index.row()].fld_act_[index.column()].col_act_ = BlazeSbsEntryActRwt;
        data_[index.row()].fld_act_[index.column()].color_timer_->start(
            VLG_TKT_INT_CELL_COLOR_RST_MSEC);

        connect(data_[index.row()].fld_act_[index.column()].color_timer_,
                SIGNAL(SignalCellResetColor(VLG_SBS_COL_DATA_ENTRY &)),
                this,
                SLOT(OnCellResetColor(VLG_SBS_COL_DATA_ENTRY &)));

        emit(dataChanged(chng_index, chng_index));
        return true;
    }
    return false;
}

bool vlg_toolkit_sbs_vlg_class_model::insertRows(int position,
                                                 int rows,
                                                 const QModelIndex &index)
{
    Q_UNUSED(index);
    beginInsertRows(QModelIndex(), position, position);
    std::shared_ptr<vlg::nclass> *insert_obj = static_cast<std::shared_ptr<vlg::nclass>*>(index.internalPointer());
    VLG_SBS_DATA_ENTRY sbs_data_entry(*insert_obj, position, colnum_);
    data_.append(sbs_data_entry);
    endInsertRows();
    return true;
}

bool vlg_toolkit_sbs_vlg_class_model::removeRows(int position, int rows,
                                                 const QModelIndex &index)
{
    return false;
}

void vlg_toolkit_sbs_vlg_class_model::GenerateModelRep()
{
    GenerateHeader();
}

void vlg_toolkit_sbs_vlg_class_model::GenerateHeader()
{
    std::string prfx;
    prfx.assign("");
    ENM_GEN_SBS_REP_REC_UD gen_rep_rud(*this, bem_, &prfx, false, 0);
    edesc_.enum_member_descriptors(enum_generate_sbs_model_rep, &gen_rep_rud);
}

void vlg_toolkit_sbs_vlg_class_model::updateEntry(int rowidx,
                                                  vlg::nclass *entry,
                                                  vlg::nclass *prev_entry)
{
    ENM_UPD_CLS_ROW_REC_UD upd_cls_row_rud(*this,
                                           bem_,
                                           (const char *)entry,
                                           (const char *)prev_entry,
                                           rowidx,
                                           false,
                                           0);

    edesc_.enum_member_descriptors(enum_update_class_row, &upd_cls_row_rud);
}

void vlg_toolkit_sbs_vlg_class_model::offerEntry(std::shared_ptr<vlg::subscription_event> &sbs_evt)
{
    int rowidx = 0;
    VLG_CLASS_ROW_IDX_PAIR crip;
    std::unique_ptr<char> entry_key;
    sbs_evt->get_data()->get_primary_key_value_as_string(entry_key);
    QString hkey(entry_key.get());
    if(data_hlpr_.contains(hkey)) {
        crip = data_hlpr_[hkey];
        rowidx = crip.rowidx_;
        updateEntry(rowidx, sbs_evt->get_data().get(), crip.obj_.get());
        crip.obj_->set_from(*sbs_evt->get_data());
    } else {
        crip.obj_ = sbs_evt->get_data()->clone();
        crip.rowidx_ = rowCount();
        insertRows(rowCount(), 1, createIndex(rowCount(), 0, &crip.obj_));
        data_hlpr_[hkey] = crip;
    }
}

void vlg_toolkit_sbs_vlg_class_model::AppendHdrColumn(const char *colname)
{
    hdr_colidx_colname_[colnum_] = QString(colname);
}

void vlg_toolkit_sbs_vlg_class_model::IncrColnum()
{
    colnum_++;
}

QHash<int, QString> &vlg_toolkit_sbs_vlg_class_model::hdr_colidx_colname()
{
    return hdr_colidx_colname_;
}
