/*
 *
 * (C) 2017 - giuseppe.baccini@gmail.com
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#include "blaze_logger.h"
#include "blaze_model.h"
#include "blz_glob_int.h"

#ifdef WIN32
#define SF_GBB_READ_H(bbuf, meth, cmd)\
{\
    blaze::RetCode gbb_read_res;\
    if((gbb_read_res = bbuf->meth)){\
        FILE *ferr = fopen("log.err", "wa+");\
        fprintf(ferr ? ferr : stderr, "GBB OVERFL ERROR (%d): cpcty:%u pos:%u limit:%u mark:%u \n",\
                gbb_read_res,\
                bbuf->capacity(),\
                bbuf->position(),\
                bbuf->limit(),\
                bbuf->mark());\
        if(ferr) fclose(ferr);\
        cmd;\
    }\
}
#else
#define SF_GBB_READ_H(bbuf, meth, cmd)\
{\
    blaze::RetCode gbb_read_res;\
    if((gbb_read_res = bbuf->meth)){\
        FILE *ferr = fopen("log.err", "wa+");\
        fprintf(ferr ? ferr : stderr, "GBB OVERFL ERROR (%d): cpcty:%zu pos:%zu limit:%zu mark:%zu \n",\
                gbb_read_res,\
                bbuf->capacity(),\
                bbuf->position(),\
                bbuf->limit(),\
                bbuf->mark());\
        if(ferr) fclose(ferr);\
        cmd;\
    }\
}
#endif

namespace blaze {

//-----------------------------
// FldSeqA_Restore
//-----------------------------
inline blaze::RetCode FldSeqA_Restore(void *entity_ptr,
                                      const entity_manager *em,
                                      Encode enctyp,
                                      const member_desc *mmbrd,
                                      blaze::grow_byte_buffer *ibb)
{
    size_t array_sz = 0, start_pos = 0;
    unsigned short array_idx = 0;
    char *elem_cptr = NULL;
    //read current array len
    SF_GBB_READ_H(ibb, read_uint_to_sizet(&array_sz), return gbb_read_res)
    if(ibb->available_read() < array_sz) {
        return blaze::RetCode_MALFORM;
    }
    //we set starting point in the buffer.
    start_pos = ibb->position();
    if(mmbrd->get_field_entity_type() == EntityType_NCLASS) {
        //class
        while((ibb->position() - start_pos) < array_sz /*&& ibb->AvlRead()*/) {
            //read elem idx.
            SF_GBB_READ_H(ibb, read_ushort(&array_idx), return gbb_read_res)
            if(mmbrd->get_field_nmemb() < array_idx) {
                return blaze::RetCode_MALFORM;
            }
            //compute array fld offset
            elem_cptr = reinterpret_cast<char *>(entity_ptr);
            elem_cptr += (mmbrd->get_field_offset() + mmbrd->get_field_type_size()
                          *array_idx);
            nclass *sub_ent = reinterpret_cast<nclass *>(elem_cptr);
            RETURN_IF_NOT_OK(sub_ent->restore(em, enctyp, ibb))
        }
    } else {
        //primitive type or enum
        while((ibb->position() - start_pos) < array_sz /*&& ibb->AvlRead()*/) {
            //read elem idx.
            SF_GBB_READ_H(ibb, read_ushort(&array_idx), return gbb_read_res)
            if(mmbrd->get_field_nmemb() < array_idx) {
                return blaze::RetCode_MALFORM;
            }
            //compute array fld offset
            elem_cptr = reinterpret_cast<char *>(entity_ptr);
            elem_cptr += (mmbrd->get_field_offset() + mmbrd->get_field_type_size()
                          *array_idx);
            SF_GBB_READ_H(ibb, read(mmbrd->get_field_type_size(), elem_cptr),
                          return gbb_read_res)
        }
    }
    return blaze::RetCode_OK;
}

//-----------------------------
// nclass
//-----------------------------
blaze::RetCode nclass::restore(const entity_manager *em,
                               Encode enctyp,
                               blaze::grow_byte_buffer *ibb)
{
    size_t obj_sz = 0, fld_sz = 0, start_pos = 0;
    unsigned short fld_idx = 0;
    char *fld_cptr = NULL;
    switch(enctyp) {
        case Encode_INDEXED_NOT_ZERO:
            //read class obj len
            SF_GBB_READ_H(ibb, read_uint_to_sizet(&obj_sz), return gbb_read_res)
            if(ibb->available_read() < obj_sz) {
                return blaze::RetCode_MALFORM;
            }
            //we set starting point in the buffer.
            start_pos = ibb->position();
            //restore field sequence.
            while((ibb->position() - start_pos) < obj_sz /*&& ibb->AvlRead()*/) {
                //read fld idx.
                SF_GBB_READ_H(ibb, read_ushort(&fld_idx), return gbb_read_res)
                //get member descriptor.
                const member_desc *mmbrd = get_entity_descriptor()->get_member_desc_by_id(
                                               fld_idx);
                if(!mmbrd) {
                    return blaze::RetCode_MALFORM;
                }
                //compute fld offset
                fld_cptr = reinterpret_cast<char *>(this);
                fld_cptr += mmbrd->get_field_offset();
                if(mmbrd->get_field_blz_type() != Type_ENTITY) {
                    //primitive type.
                    if(mmbrd->get_field_blz_type() == Type_ASCII &&
                            mmbrd->get_field_nmemb() > 1) {
                        //strings
                        //read string len.
                        SF_GBB_READ_H(ibb, read_uint_to_sizet(&fld_sz), return gbb_read_res)
                        if(mmbrd->get_field_nmemb() < fld_sz) {
                            return blaze::RetCode_MALFORM;
                        }
                        SF_GBB_READ_H(ibb, read(fld_sz, fld_cptr), return gbb_read_res)
                    } else {
                        if(mmbrd->get_field_nmemb() > 1) {
                            RETURN_IF_NOT_OK(FldSeqA_Restore(this, em, enctyp, mmbrd, ibb))
                        } else {
                            fld_sz = mmbrd->get_field_type_size();
                            SF_GBB_READ_H(ibb, read(fld_sz, fld_cptr), return gbb_read_res)
                        }
                    }
                } else {
                    //entity.
                    switch(mmbrd->get_field_entity_type()) {
                        case EntityType_ENUM:
                            //treat enum as a primitive type.
                            if(mmbrd->get_field_nmemb() > 1) {
                                RETURN_IF_NOT_OK(FldSeqA_Restore(this, em, enctyp, mmbrd, ibb))
                            } else {
                                fld_sz = mmbrd->get_field_type_size();
                                SF_GBB_READ_H(ibb, read(fld_sz, fld_cptr), return gbb_read_res)
                            }
                            break;
                        case EntityType_NCLASS:
                            //class.
                            if(mmbrd->get_field_nmemb() > 1) {
                                RETURN_IF_NOT_OK(FldSeqA_Restore(this, em, enctyp, mmbrd, ibb))
                            } else {
                                nclass *sub_ent = reinterpret_cast<nclass *>(fld_cptr);
                                RETURN_IF_NOT_OK(sub_ent->restore(em, enctyp, ibb))
                            }
                            break;
                        default:
                            return blaze::RetCode_MALFORM;
                    }
                }
            }
            break;
        default:
            return blaze::RetCode_UNSP;
    }
    return blaze::RetCode_OK;
}

}

