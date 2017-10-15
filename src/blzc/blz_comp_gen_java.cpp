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

#include "blz_compiler.h"

#define BLZ_COMP_JAVA_GETTER_PFX         "get"
#define BLZ_COMP_JAVA_SETTER_PFX         "set"

namespace blaze {

extern blaze::ascii_string unit_nmspace;

/***********************************
- isJavaStringType
***********************************/
bool isJavaStringType(member_desc_comp *mdsc)
{
    return
        mdsc->get_nmemb() > 1 &&
        (mdsc->get_field_type() == Type_ASCII);
}

/***********************************
- rec_brackets_cond
***********************************/
const char *rec_brackets_cond(member_desc_comp *mdsc)
{
    if(!(mdsc->get_field_type() == Type_ASCII)) {
        return "[]";
    }
    return "";
}

/***********************************
RENDER- BLZ_COMP_JAVA_BLZ_Entity_Ctor
***********************************/
blaze::RetCode BLZ_COMP_JAVA_BLZ_Entity_Ctor(compile_unit &cunit,
                                             entity_desc_comp &edsc,
                                             FILE *file)
{
    fprintf(file,  "/*****************************************************\n"
            "ctor\n"
            "*****************************************************/\n");
    fprintf(file, "public %s(){\n", edsc.get_entity_name());
    blaze::hash_map &entitymap = cunit.get_entity_map();
    blaze::hash_map &mmbr_map = edsc.get_map_id_MMBRDSC();
    mmbr_map.start_iteration();
    member_desc_comp *mdsc = NULL;
    while(!mmbr_map.next(NULL, &mdsc)) {
        if(mdsc->get_member_type() != MemberType_FIELD) {
            continue;
        }
        if(mdsc->get_field_type() == Type_ENTITY) {
            entity_desc_comp *inner_edsc = NULL;
            if(!entitymap.get(mdsc->get_field_usr_str_type(), &inner_edsc)) {
                if(inner_edsc->get_entity_type() == EntityType_ENUM) {
                    if(mdsc->get_nmemb() == 1) {
                        //enum  nmemb == 1
                        fprintf(file, CR_1IND"%s = new %s();\n", mdsc->get_member_name(),
                                inner_edsc->get_entity_name());
                    } else {
                        //enum  nmemb > 1
                        fprintf(file, CR_1IND"%s = new %s[%lu];\n", mdsc->get_member_name(),
                                inner_edsc->get_entity_name(),
                                mdsc->get_nmemb());
                        fprintf(file, CR_1IND"for(int i=0; i<%s.length; i++){\n",
                                mdsc->get_member_name());
                        fprintf(file, CR_2IND"%s[i] = new %s();\n", mdsc->get_member_name(),
                                inner_edsc->get_entity_name());
                        fprintf(file, CR_1IND"}\n");
                    }
                } else {
                    if(mdsc->get_nmemb() == 1) {
                        //class, struct  nmemb == 1
                        fprintf(file, CR_1IND"%s = new %s();\n", mdsc->get_member_name(),
                                inner_edsc->get_entity_name());
                    } else {
                        //class, struct  nmemb > 1
                        fprintf(file, CR_1IND"%s = new %s[%lu];\n", mdsc->get_member_name(),
                                inner_edsc->get_entity_name(),
                                mdsc->get_nmemb());
                        fprintf(file, CR_1IND"for(int i=0; i<%s.length; i++){\n",
                                mdsc->get_member_name());
                        fprintf(file, CR_2IND"%s[i] = new %s();\n", mdsc->get_member_name(),
                                inner_edsc->get_entity_name());
                        fprintf(file, CR_1IND"}\n");
                    }
                }
            }
        } else if(mdsc->get_nmemb() == 1) {
            //primitive type nmemb == 1
            blaze::ascii_string ttype, zero_val;
            RETURN_IF_NOT_OK(target_type_from_builtin_BLZ_TYPE(*mdsc, ttype))
            RETURN_IF_NOT_OK(get_zero_val_for_BLZ_TYPE(mdsc->get_field_type(), zero_val))
            fprintf(file, CR_1IND"%s = new %s(%s);\n", mdsc->get_member_name(),
                    ttype.internal_buff(),
                    zero_val.internal_buff());
        } else {
            //primitive type nmemb > 1
            if(isJavaStringType(mdsc)) {
                //strings
                fprintf(file, CR_1IND"%s = new String();\n", mdsc->get_member_name());
            } else {
                //primitive type nmemb > 1
                blaze::ascii_string ttype, zero_val;
                RETURN_IF_NOT_OK(target_type_from_builtin_BLZ_TYPE(*mdsc, ttype))
                RETURN_IF_NOT_OK(get_zero_val_for_BLZ_TYPE(mdsc->get_field_type(), zero_val))
                fprintf(file, CR_1IND"%s = new %s[%lu];\n", mdsc->get_member_name(),
                        ttype.internal_buff(),
                        mdsc->get_nmemb());
                fprintf(file, CR_1IND"for(int i=0; i<%s.length; i++){\n",
                        mdsc->get_member_name());
                fprintf(file, CR_2IND"%s[i] = new %s(%s);\n", mdsc->get_member_name(),
                        ttype.internal_buff(),
                        zero_val.internal_buff());
                fprintf(file, CR_1IND"}\n");
            }
        }
    }
    fprintf(file, "}");
    return blaze::RetCode_OK;
}

/*************************************************************
java BlazeEntity abstract meths
**************************************************************/
#define ABSR_METH_ENT_COPY "public void copy(BlazeEntity out){"
#define ABSR_METH_ENT_CLONE "public BlazeEntity clone(){"
#define ABSR_METH_ENT_ISZERO "public boolean isZero(){"
#define ABSR_METH_ENT_SETZERO "public void setZero(){"
#define ABSR_METH_ENT_SET "public void set(BlazeEntity in){"
#define ABSR_METH_ENT_GCOMPVER "public int getCompile_Ver(){"
#define ABSR_METH_ENT_GENTDSC "public BlazeEntityDesc getEntityDesc(){"
#define ABSR_METH_ENT_PTOBUF "public int printToBuff(StringBuffer buff, boolean print_cname, int mode){"
#define ABSR_METH_ENT_SER "public int serialize(int enctyp, BlazeEntity prev_image, BlazeByteBuffer out_buf){"


/*************************************************************
java BlazeClass abstract meths
**************************************************************/
#define ABSR_METH_GETENTID "public int getClassId(){"


/***********************************
RENDER- ABSR_METH_ENT_COPY__Body_impl
***********************************/
blaze::RetCode ABSR_METH_ENT_COPY__Body_impl(compile_unit &cunit,
                                             entity_desc_comp &edsc,
                                             FILE *file)
{
    fprintf(file, CR_1IND"if(!(out instanceof %s)){\n"
            CR_2IND"throw new IllegalArgumentException(\"not instanceof %s\");\n"
            CR_1IND"}\n", edsc.get_entity_name(), edsc.get_entity_name());
    fprintf(file, CR_1IND"%s cstout = (%s)out;\n", edsc.get_entity_name(),
            edsc.get_entity_name());
    blaze::hash_map &entitymap = cunit.get_entity_map();
    blaze::hash_map &mmbr_map = edsc.get_map_id_MMBRDSC();
    mmbr_map.start_iteration();
    member_desc_comp *mdsc = NULL;
    while(!mmbr_map.next(NULL, &mdsc)) {
        if(mdsc->get_member_type() != MemberType_FIELD) {
            continue;
        }
        if(mdsc->get_field_type() == Type_ENTITY) {
            entity_desc_comp *inner_edsc = NULL;
            if(!entitymap.get(mdsc->get_field_usr_str_type(), &inner_edsc)) {
                if(inner_edsc->get_entity_type() == EntityType_ENUM) {
                    if(mdsc->get_nmemb() == 1) {
                        //enum  nmemb == 1
                        fprintf(file, CR_1IND"cstout.%s.setValue(%s.getValue());\n",
                                mdsc->get_member_name(), mdsc->get_member_name());
                    } else {
                        //enum  nmemb > 1
                        fprintf(file, CR_1IND"for(int i=0; i<%s.length; i++){\n",
                                mdsc->get_member_name());
                        fprintf(file, CR_2IND"cstout.%s[i].setValue(%s[i].getValue());\n",
                                mdsc->get_member_name(), mdsc->get_member_name());
                        fprintf(file, CR_1IND"}\n");
                    }
                } else {
                    if(mdsc->get_nmemb() == 1) {
                        //class, struct  nmemb == 1
                        fprintf(file, CR_2IND"%s.copy(cstout.%s);\n", mdsc->get_member_name(),
                                mdsc->get_member_name());
                    } else {
                        //class, struct  nmemb > 1
                        fprintf(file, CR_1IND"for(int i=0; i<%s.length; i++){\n",
                                mdsc->get_member_name());
                        fprintf(file, CR_2IND"%s[i].copy(cstout.%s[i]);\n", mdsc->get_member_name(),
                                mdsc->get_member_name());
                        fprintf(file, CR_1IND"}\n");
                    }
                }
            }
        } else if(mdsc->get_nmemb() == 1) {
            //primitive type nmemb == 1
            fprintf(file, CR_1IND"cstout.%s = %s;\n", mdsc->get_member_name(),
                    mdsc->get_member_name());
        } else {
            //primitive type nmemb > 1
            if(isJavaStringType(mdsc)) {
                //strings
                fprintf(file, CR_1IND"cstout.%s = %s;\n", mdsc->get_member_name(),
                        mdsc->get_member_name());
            } else {
                //primitive type nmemb > 1
                fprintf(file, CR_1IND"for(int i=0; i<%s.length; i++){\n",
                        mdsc->get_member_name());
                fprintf(file, CR_2IND"cstout.%s[i] = %s[i];\n", mdsc->get_member_name(),
                        mdsc->get_member_name());
                fprintf(file, CR_1IND"}\n");
            }
        }
    }
    return blaze::RetCode_OK;
}

/***********************************
RENDER- ABSR_METH_ENT_CLONE__Body_impl
***********************************/
blaze::RetCode ABSR_METH_ENT_CLONE__Body_impl(compile_unit &cunit,
                                              entity_desc_comp &edsc,
                                              FILE *file)
{
    fprintf(file, CR_1IND"BlazeEntity ne = new %s();\n", edsc.get_entity_name());
    fprintf(file, CR_1IND"ne.set(this);\n");
    fprintf(file, CR_1IND"return ne;\n");
    return blaze::RetCode_OK;
}

/***********************************
RENDER- ABSR_METH_ENT_ISZERO__Body_impl
***********************************/
blaze::RetCode ABSR_METH_ENT_ISZERO__Body_impl(compile_unit &cunit,
                                               entity_desc_comp &edsc,
                                               FILE *file)
{
    blaze::ascii_string zero_val;
    blaze::hash_map &entitymap = cunit.get_entity_map();
    blaze::hash_map &mmbr_map = edsc.get_map_id_MMBRDSC();
    mmbr_map.start_iteration();
    member_desc_comp *mdsc = NULL;
    while(!mmbr_map.next(NULL, &mdsc)) {
        if(mdsc->get_member_type() != MemberType_FIELD) {
            continue;
        }
        if(mdsc->get_field_type() == Type_ENTITY) {
            entity_desc_comp *inner_edsc = NULL;
            if(!entitymap.get(mdsc->get_field_usr_str_type(), &inner_edsc)) {
                if(inner_edsc->get_entity_type() == EntityType_ENUM) {
                    if(mdsc->get_nmemb() == 1) {
                        //enum  nmemb == 1
                        fprintf(file, CR_1IND"if(%s.getValue() != 0) return false;\n",
                                mdsc->get_member_name());
                    } else {
                        //enum  nmemb > 1
                        fprintf(file, CR_1IND"for(int i=0; i<%s.length; i++){\n",
                                mdsc->get_member_name());
                        fprintf(file, CR_2IND"if(%s[i].getValue() != 0) return false;\n",
                                mdsc->get_member_name());
                        fprintf(file, CR_1IND"}\n");
                    }
                } else {
                    if(mdsc->get_nmemb() == 1) {
                        //class, struct  nmemb == 1
                        fprintf(file, CR_2IND"if(!%s.isZero()) return false;\n",
                                mdsc->get_member_name());
                    } else {
                        //class, struct  nmemb > 1
                        fprintf(file, CR_1IND"for(int i=0; i<%s.length; i++){\n",
                                mdsc->get_member_name());
                        fprintf(file, CR_2IND"if(!%s[i].isZero()) return false;\n",
                                mdsc->get_member_name());
                        fprintf(file, CR_1IND"}\n");
                    }
                }
            }
        } else if(mdsc->get_nmemb() == 1) {
            //primitive type nmemb == 1
            RETURN_IF_NOT_OK(get_zero_val_for_BLZ_TYPE(mdsc->get_field_type(), zero_val))
            fprintf(file, CR_1IND"if(%s != %s) return false;\n", mdsc->get_member_name(),
                    zero_val.internal_buff());
        } else {
            //primitive type nmemb > 1
            if(isJavaStringType(mdsc)) {
                //strings
                fprintf(file, CR_1IND"if(!\"\".equals(%s)) return false;\n",
                        mdsc->get_member_name());
            } else {
                //primitive type nmemb > 1
                RETURN_IF_NOT_OK(get_zero_val_for_BLZ_TYPE(mdsc->get_field_type(), zero_val))
                fprintf(file, CR_1IND"for(int i=0; i<%s.length; i++){\n",
                        mdsc->get_member_name());
                fprintf(file, CR_2IND"if(%s[i] != %s) return false;\n", mdsc->get_member_name(),
                        zero_val.internal_buff());
                fprintf(file, CR_1IND"}\n");
            }
        }
    }
    fprintf(file, CR_1IND"return true;\n");
    return blaze::RetCode_OK;
}

/***********************************
RENDER- ABSR_METH_ENT_SETZERO__Body_impl
***********************************/
blaze::RetCode ABSR_METH_ENT_SETZERO__Body_impl(compile_unit &cunit,
                                                entity_desc_comp &edsc,
                                                FILE *file)
{
    blaze::ascii_string zero_val;
    blaze::hash_map &entitymap = cunit.get_entity_map();
    blaze::hash_map &mmbr_map = edsc.get_map_id_MMBRDSC();
    mmbr_map.start_iteration();
    member_desc_comp *mdsc = NULL;
    while(!mmbr_map.next(NULL, &mdsc)) {
        if(mdsc->get_member_type() != MemberType_FIELD) {
            continue;
        }
        if(mdsc->get_field_type() == Type_ENTITY) {
            entity_desc_comp *inner_edsc = NULL;
            if(!entitymap.get(mdsc->get_field_usr_str_type(), &inner_edsc)) {
                if(inner_edsc->get_entity_type() == EntityType_ENUM) {
                    if(mdsc->get_nmemb() == 1) {
                        //enum  nmemb == 1
                        fprintf(file, CR_1IND"%s.setValue(0);\n", mdsc->get_member_name());
                    } else {
                        //enum  nmemb > 1
                        fprintf(file, CR_1IND"for(int i=0; i<%s.length; i++){\n",
                                mdsc->get_member_name());
                        fprintf(file, CR_2IND"%s[i].setValue(0);\n", mdsc->get_member_name());
                        fprintf(file, CR_1IND"}\n");
                    }
                } else {
                    if(mdsc->get_nmemb() == 1) {
                        //class, struct  nmemb == 1
                        fprintf(file, CR_2IND"%s.setZero();\n", mdsc->get_member_name());
                    } else {
                        //class, struct  nmemb > 1
                        fprintf(file, CR_1IND"for(int i=0; i<%s.length; i++){\n",
                                mdsc->get_member_name());
                        fprintf(file, CR_2IND"%s[i].setZero();\n", mdsc->get_member_name());
                        fprintf(file, CR_1IND"}\n");
                    }
                }
            }
        } else if(mdsc->get_nmemb() == 1) {
            //primitive type nmemb == 1
            RETURN_IF_NOT_OK(get_zero_val_for_BLZ_TYPE(mdsc->get_field_type(), zero_val))
            fprintf(file, CR_1IND"%s = %s;\n", mdsc->get_member_name(),
                    zero_val.internal_buff());
        } else {
            //primitive type nmemb > 1
            if(isJavaStringType(mdsc)) {
                //strings
                fprintf(file, CR_1IND"%s = \"\";\n", mdsc->get_member_name());
            } else {
                //primitive type nmemb > 1
                RETURN_IF_NOT_OK(get_zero_val_for_BLZ_TYPE(mdsc->get_field_type(), zero_val))
                fprintf(file, CR_1IND"for(int i=0; i<%s.length; i++){\n",
                        mdsc->get_member_name());
                fprintf(file, CR_2IND"%s[i] = %s;\n", mdsc->get_member_name(),
                        zero_val.internal_buff());
                fprintf(file, CR_1IND"}\n");
            }
        }
    }
    return blaze::RetCode_OK;
}

/***********************************
RENDER- ABSR_METH_ENT_SET__Body_impl
***********************************/
blaze::RetCode ABSR_METH_ENT_SET__Body_impl(compile_unit &cunit,
                                            entity_desc_comp &edsc,
                                            FILE *file)
{
    fprintf(file, CR_1IND"if(!(in instanceof %s)){\n"
            CR_2IND"throw new IllegalArgumentException(\"not instanceof %s\");\n"
            CR_1IND"}\n", edsc.get_entity_name(), edsc.get_entity_name());
    fprintf(file, CR_1IND"%s cstin = (%s)in;\n", edsc.get_entity_name(),
            edsc.get_entity_name());
    blaze::hash_map &entitymap = cunit.get_entity_map();
    blaze::hash_map &mmbr_map = edsc.get_map_id_MMBRDSC();
    mmbr_map.start_iteration();
    member_desc_comp *mdsc = NULL;
    while(!mmbr_map.next(NULL, &mdsc)) {
        if(mdsc->get_member_type() != MemberType_FIELD) {
            continue;
        }
        if(mdsc->get_field_type() == Type_ENTITY) {
            entity_desc_comp *inner_edsc = NULL;
            if(!entitymap.get(mdsc->get_field_usr_str_type(), &inner_edsc)) {
                if(inner_edsc->get_entity_type() == EntityType_ENUM) {
                    if(mdsc->get_nmemb() == 1) {
                        //enum  nmemb == 1
                        fprintf(file, CR_1IND"%s.setValue(cstin.%s.getValue());\n",
                                mdsc->get_member_name(), mdsc->get_member_name());
                    } else {
                        //enum  nmemb > 1
                        fprintf(file, CR_1IND"for(int i=0; i<%s.length; i++){\n",
                                mdsc->get_member_name());
                        fprintf(file, CR_2IND"%s[i].setValue(cstin.%s[i].getValue());\n",
                                mdsc->get_member_name(), mdsc->get_member_name());
                        fprintf(file, CR_1IND"}\n");
                    }
                } else {
                    if(mdsc->get_nmemb() == 1) {
                        //class, struct  nmemb == 1
                        fprintf(file, CR_2IND"%s.set(cstin.%s);\n", mdsc->get_member_name(),
                                mdsc->get_member_name());
                    } else {
                        //class, struct  nmemb > 1
                        fprintf(file, CR_1IND"for(int i=0; i<%s.length; i++){\n",
                                mdsc->get_member_name());
                        fprintf(file, CR_2IND"%s[i].set(cstin.%s[i]);\n", mdsc->get_member_name(),
                                mdsc->get_member_name());
                        fprintf(file, CR_1IND"}\n");
                    }
                }
            }
        } else if(mdsc->get_nmemb() == 1) {
            //primitive type nmemb == 1
            fprintf(file, CR_1IND"%s = cstin.%s;\n", mdsc->get_member_name(),
                    mdsc->get_member_name());
        } else {
            //primitive type nmemb > 1
            if(isJavaStringType(mdsc)) {
                //strings
                fprintf(file, CR_1IND"%s = cstin.%s;\n", mdsc->get_member_name(),
                        mdsc->get_member_name());
            } else {
                //primitive type nmemb > 1
                fprintf(file, CR_1IND"for(int i=0; i<%s.length; i++){\n",
                        mdsc->get_member_name());
                fprintf(file, CR_2IND"%s[i] = cstin.%s[i];\n", mdsc->get_member_name(),
                        mdsc->get_member_name());
                fprintf(file, CR_1IND"}\n");
            }
        }
    }
    return blaze::RetCode_OK;
}

/***********************************
RENDER- ABSR_METH_ENT_GCOMPVER__Body_impl
***********************************/
blaze::RetCode ABSR_METH_ENT_GCOMPVER__Body_impl(compile_unit &cunit,
                                                 entity_desc_comp &edsc,
                                                 FILE *file)
{
    fprintf(file, CR_1IND"return 0;\n");
    return blaze::RetCode_OK;
}

/***********************************
RENDER- ABSR_METH_ENT_GENTDSC__Body_impl
***********************************/
blaze::RetCode ABSR_METH_ENT_GENTDSC__Body_impl(compile_unit &cunit,
                                                entity_desc_comp &edsc,
                                                FILE *file)
{
    blaze::ascii_string ep_nm;
    ep_nm.append(cunit.model_name());
    fprintf(file, CR_1IND"return %s.getBem().getEntityDesc(\"%s\");\n",
            ep_nm.internal_buff(),
            edsc.get_entity_name());
    return blaze::RetCode_OK;
}

/***********************************
RENDER- ABSR_METH_ENT_PTOBUF__NotZeroMode
***********************************/
blaze::RetCode ABSR_METH_ENT_PTOBUF__NotZeroMode(compile_unit &cunit,
                                                 entity_desc_comp &edsc,
                                                 FILE *file)
{
    blaze::ascii_string mb_nm;
    blaze::hash_map &entitymap = cunit.get_entity_map();
    blaze::hash_map &mmbr_map = edsc.get_map_id_MMBRDSC();
    mmbr_map.start_iteration();
    member_desc_comp *mdsc = NULL;
    while(!mmbr_map.next(NULL, &mdsc)) {
        if(mdsc->get_member_type() != MemberType_FIELD) {
            continue;
        }
        RETURN_IF_NOT_OK(mb_nm.assign(mdsc->get_member_name()))
        mb_nm.first_char_uppercase();
        if(mdsc->get_field_type() == Type_ENTITY) {
            entity_desc_comp *inner_edsc = NULL;
            if(!entitymap.get(mdsc->get_field_usr_str_type(), &inner_edsc)) {
                if(inner_edsc->get_entity_type() == EntityType_ENUM) {
                    if(mdsc->get_nmemb() == 1) {
                        //enum  nmemb == 1
                        fprintf(file, CR_2IND
                                "if(!isZero%s()){ buff.append(\"%s=\"); buff.append(%s.getValue()); buff.append(\"|\"); }\n",
                                mb_nm.internal_buff(),
                                mdsc->get_member_name(),
                                mdsc->get_member_name());
                    } else {
                        //enum  nmemb > 1
                        fprintf(file, CR_2IND "if(!isZero%s()){\n", mb_nm.internal_buff());
                        fprintf(file, CR_3IND"frst_test = true;\n");
                        fprintf(file, CR_3IND"buff.append(\"%s=\");\n", mdsc->get_member_name());
                        fprintf(file, CR_3IND"buff.append(\"[\");\n");
                        fprintf(file, CR_3IND"for(int i=0; i<%s.length; i++){\n",
                                mdsc->get_member_name());
                        fprintf(file,
                                CR_4IND"if(frst_test){frst_test = false;}else{ buff.append(\",\");}\n");
                        fprintf(file, CR_4IND"buff.append(%s[i].getValue());\n",
                                mdsc->get_member_name());
                        fprintf(file, CR_4IND"buff.append(\",\");\n");
                        fprintf(file, CR_3IND"}\n");
                        fprintf(file, CR_3IND"buff.append(\"]\");\n");
                        fprintf(file, CR_2IND"}\n");
                    }
                } else {
                    if(mdsc->get_nmemb() == 1) {
                        //class, struct  nmemb == 1
                        fprintf(file, CR_2IND
                                "if(!isZero%s()){ buff.append(\"%s=\"); %s.printToBuff(buff, false, mode); buff.append(\"|\"); }\n",
                                mb_nm.internal_buff(),
                                mdsc->get_member_name(),
                                mdsc->get_member_name());
                    } else {
                        //class, struct  nmemb > 1
                        fprintf(file, CR_2IND "if(!isZero%s()){\n", mb_nm.internal_buff());
                        fprintf(file, CR_4IND"frst_test = true;\n");
                        fprintf(file, CR_3IND"buff.append(\"%s=\");\n", mdsc->get_member_name());
                        fprintf(file, CR_3IND"buff.append(\"[\");\n");
                        fprintf(file, CR_3IND"for(int i=0; i<%s.length; i++){\n",
                                mdsc->get_member_name());
                        fprintf(file,
                                CR_4IND"if(frst_test){frst_test = false;}else{ buff.append(\",\");}\n");
                        fprintf(file, CR_4IND "%s[i].printToBuff(buff, false, mode);\n",
                                mdsc->get_member_name());
                        fprintf(file, CR_3IND"}\n");
                        fprintf(file, CR_3IND"buff.append(\"]\");\n");
                        fprintf(file, CR_2IND"}\n");
                    }
                }
            }
        } else if(mdsc->get_nmemb() == 1) {
            //primitive type nmemb == 1
            fprintf(file, CR_2IND
                    "if(!isZero%s()){ buff.append(\"%s=\"); buff.append(%s); buff.append(\"|\"); }\n",
                    mb_nm.internal_buff(),
                    mdsc->get_member_name(),
                    mdsc->get_member_name());
        } else {
            //primitive type nmemb > 1
            if(isJavaStringType(mdsc)) {
                //strings
                fprintf(file, CR_2IND
                        "if(!isZero%s()){ buff.append(\"%s=[\"); buff.append(%s); buff.append(\"]\"); }\n",
                        mb_nm.internal_buff(),
                        mdsc->get_member_name(),
                        mdsc->get_member_name());
            } else {
                //primitive type nmemb > 1
                fprintf(file, CR_2IND "if(!isZero%s()){\n", mb_nm.internal_buff());
                fprintf(file, CR_2IND"frst_test = true;\n");
                fprintf(file, CR_3IND"buff.append(\"%s=\");\n", mdsc->get_member_name());
                fprintf(file, CR_3IND"buff.append(\"[\");\n");
                fprintf(file, CR_3IND"for(int i=0; i<%s.length; i++){\n",
                        mdsc->get_member_name());
                fprintf(file,
                        CR_4IND"if(frst_test){frst_test = false;}else{ buff.append(\",\");}\n");
                fprintf(file, CR_4IND"buff.append(%s[i]);\n", mdsc->get_member_name());
                fprintf(file, CR_3IND"}\n");
                fprintf(file, CR_3IND"buff.append(\"]\");\n");
                fprintf(file, CR_2IND"}\n");
            }
        }
    }
    return blaze::RetCode_OK;
}

/***********************************
RENDER- ABSR_METH_ENT_PTOBUF__AllFildMode
***********************************/
blaze::RetCode ABSR_METH_ENT_PTOBUF__AllFildMode(compile_unit &cunit,
                                                 entity_desc_comp &edsc,
                                                 FILE *file)
{
    blaze::hash_map &entitymap = cunit.get_entity_map();
    blaze::hash_map &mmbr_map = edsc.get_map_id_MMBRDSC();
    mmbr_map.start_iteration();
    member_desc_comp *mdsc = NULL;
    while(!mmbr_map.next(NULL, &mdsc)) {
        if(mdsc->get_member_type() != MemberType_FIELD) {
            continue;
        }
        if(mdsc->get_field_type() == Type_ENTITY) {
            entity_desc_comp *inner_edsc = NULL;
            if(!entitymap.get(mdsc->get_field_usr_str_type(), &inner_edsc)) {
                if(inner_edsc->get_entity_type() == EntityType_ENUM) {
                    if(mdsc->get_nmemb() == 1) {
                        //enum  nmemb == 1
                        fprintf(file, CR_2IND
                                "buff.append(\"%s=\"); buff.append(%s.getValue()); buff.append(\"|\");\n",
                                mdsc->get_member_name(),
                                mdsc->get_member_name());
                    } else {
                        //enum  nmemb > 1
                        fprintf(file, CR_2IND"buff.append(\"%s=\");\n", mdsc->get_member_name());
                        fprintf(file, CR_2IND"buff.append(\"[\");\n");
                        fprintf(file, CR_2IND"for(int i=0; i<%s.length; i++){\n",
                                mdsc->get_member_name());
                        fprintf(file, CR_3IND"buff.append(%s[i].getValue());\n",
                                mdsc->get_member_name());
                        fprintf(file, CR_3IND"buff.append(\",\");\n");
                        fprintf(file, CR_2IND"}\n");
                        fprintf(file, CR_2IND"buff.append(\"]\");\n");
                    }
                } else {
                    if(mdsc->get_nmemb() == 1) {
                        //class, struct  nmemb == 1
                        fprintf(file, CR_2IND
                                "buff.append(\"%s=\"); %s.printToBuff(buff, false, mode); buff.append(\"|\");\n",
                                mdsc->get_member_name(),
                                mdsc->get_member_name());
                    } else {
                        //class, struct  nmemb > 1
                        fprintf(file, CR_2IND"buff.append(\"%s=\");\n", mdsc->get_member_name());
                        fprintf(file, CR_2IND"buff.append(\"[\");\n");
                        fprintf(file, CR_2IND"for(int i=0; i<%s.length; i++){\n",
                                mdsc->get_member_name());
                        fprintf(file, CR_3IND "%s[i].printToBuff(buff, false, mode);\n",
                                mdsc->get_member_name());
                        fprintf(file, CR_2IND"}\n");
                        fprintf(file, CR_2IND"buff.append(\"]\");\n");
                    }
                }
            }
        } else if(mdsc->get_nmemb() == 1) {
            //primitive type nmemb == 1
            fprintf(file, CR_2IND
                    "buff.append(\"%s=\"); buff.append(%s); buff.append(\"|\");\n",
                    mdsc->get_member_name(),
                    mdsc->get_member_name());
        } else {
            //primitive type nmemb > 1
            if(isJavaStringType(mdsc)) {
                //strings
                fprintf(file, CR_2IND
                        "buff.append(\"%s=\"); buff.append(%s); buff.append(\"|\");\n",
                        mdsc->get_member_name(),
                        mdsc->get_member_name());
            } else {
                //primitive type nmemb > 1
                fprintf(file, CR_2IND"buff.append(\"%s=\");\n", mdsc->get_member_name());
                fprintf(file, CR_2IND"buff.append(\"[\");\n");
                fprintf(file, CR_2IND"for(int i=0; i<%s.length; i++){\n",
                        mdsc->get_member_name());
                fprintf(file, CR_3IND"buff.append(%s[i]);\n", mdsc->get_member_name());
                fprintf(file, CR_3IND"buff.append(\",\");\n");
                fprintf(file, CR_2IND"}\n");
                fprintf(file, CR_2IND"buff.append(\"]\");\n");
            }
        }
    }
    return blaze::RetCode_OK;
}

/***********************************
RENDER- ABSR_METH_ENT_PTOBUF__Body_impl
***********************************/
blaze::RetCode ABSR_METH_ENT_PTOBUF__Body_impl(compile_unit &cunit,
                                               entity_desc_comp &edsc,
                                               FILE *file)
{
    fprintf(file, CR_1IND "boolean frst_test = true;\n");
    fprintf(file, CR_1IND "if(print_cname) buff.append(\"%s\");\n",
            edsc.get_entity_name());
    //class opening curl brace
    fprintf(file, CR_1IND "buff.append(\"{\");\n");
    fprintf(file, CR_1IND "switch(mode){\n");
    fprintf(file, CR_2IND "case BlazeFramework.BLZ_PRINT_MODE_Undef:\n");
    fprintf(file, CR_2IND "case BlazeFramework.BLZ_PRINT_MODE_NotZero:\n");
    //NOTZERO MODE
    RETURN_IF_NOT_OK(ABSR_METH_ENT_PTOBUF__NotZeroMode(cunit, edsc, file))
    fprintf(file, CR_2IND "break;\n");
    fprintf(file, CR_2IND "case BlazeFramework.BLZ_PRINT_MODE_All:\n");
    //ALL MODE
    RETURN_IF_NOT_OK(ABSR_METH_ENT_PTOBUF__AllFildMode(cunit, edsc, file))
    fprintf(file, CR_2IND "break;\n");
    fprintf(file, CR_2IND "default:\n");
    fprintf(file, CR_2IND "break;\n");
    fprintf(file, CR_1IND "}\n");
    //class closing curl brace
    fprintf(file, CR_1IND "buff.append(\"}\");\n");
    fprintf(file, CR_1IND "return 0;\n");
    return blaze::RetCode_OK;
}

/***********************************
RENDER- ABSR_METH_ENT_SER__IndexedNotZero
***********************************/
blaze::RetCode ABSR_METH_ENT_SER__IndexedNotZero(compile_unit &cunit,
                                                 entity_desc_comp &edsc,
                                                 FILE *file)
{
    fprintf(file, CR_3IND"ByteBuffer bb = ByteBuffer.allocate(8);\n");
    fprintf(file, CR_3IND"bb.order(ByteOrder.LITTLE_ENDIAN);\n");
    fprintf(file, CR_3IND"int tlen_offst = out_buf.getPos();\n");
    fprintf(file, CR_3IND"out_buf.mvPos(BlazeFramework.ENTLN_B_SZ);\n");
    fprintf(file,
            CR_3IND"tlen = out_buf.getPos();  //we are reserving 4 bytes for total entity len.\n");
    blaze::ascii_string mb_nm;
    blaze::hash_map &entitymap = cunit.get_entity_map();
    blaze::hash_map &mmbr_map = edsc.get_map_id_MMBRDSC();
    mmbr_map.start_iteration();
    member_desc_comp *mdsc = NULL;
    while(!mmbr_map.next(NULL, &mdsc)) {
        if(mdsc->get_member_type() != MemberType_FIELD) {
            continue;
        }
        RETURN_IF_NOT_OK(mb_nm.assign(mdsc->get_member_name()))
        mb_nm.first_char_uppercase();
        fprintf(file, CR_3IND "if(!isZero%s()){\n", mb_nm.internal_buff());
        fprintf(file, CR_4IND"bb.putShort((short)%d).flip();\n"
                CR_4IND"out_buf.apndByteBuffer(bb);\n"
                CR_4IND"bb.clear();\n", mdsc->get_member_id());
        if(mdsc->get_field_type() == Type_ENTITY) {
            entity_desc_comp *inner_edsc = NULL;
            if(!entitymap.get(mdsc->get_field_usr_str_type(), &inner_edsc)) {
                if(inner_edsc->get_entity_type() == EntityType_ENUM) {
                    if(mdsc->get_nmemb() == 1) {
                        //enum  nmemb == 1
                        fprintf(file, CR_4IND"bb.putInt(%s.getValue()).flip();\n"
                                CR_4IND"out_buf.apndByteBuffer(bb);\n"
                                CR_4IND"bb.clear();\n", mdsc->get_member_name());
                    } else {
                        //enum  nmemb > 1
                        fprintf(file, CR_4IND"int alen = 0, alen_offst = out_buf.getPos();\n");
                        fprintf(file, CR_4IND"out_buf.mvPos(BlazeFramework.ARRAY_B_SZ);\n");
                        fprintf(file,
                                CR_4IND"alen = out_buf.getPos();  //we are reserving 4 bytes for array len.\n");
                        fprintf(file, CR_4IND"for(short i=0; i<%s.length; i++){\n",
                                mdsc->get_member_name());
                        fprintf(file, CR_5IND "if(get%s()[i].getValue() != 0){\n",
                                mb_nm.internal_buff());
                        fprintf(file, CR_6IND"bb.putShort(i).flip();\n"
                                CR_6IND"out_buf.apndByteBuffer(bb);\n"
                                CR_6IND"bb.clear();\n");
                        fprintf(file, CR_6IND"bb.putInt(%s[i].getValue()).flip();\n"
                                CR_6IND"out_buf.apndByteBuffer(bb);\n"
                                CR_6IND"bb.clear();\n", mdsc->get_member_name());
                        fprintf(file, CR_5IND"}\n");
                        fprintf(file, CR_4IND"}\n");
                        fprintf(file, CR_4IND"bb.putInt(out_buf.getPos() - alen).flip();\n");
                        fprintf(file,
                                CR_4IND"out_buf.putByteBuffer(bb, alen_offst, BlazeFramework.ARRAY_B_SZ);\n"
                                CR_4IND"bb.clear();\n");
                    }
                } else {
                    if(mdsc->get_nmemb() == 1) {
                        //class, struct  nmemb == 1
                        fprintf(file, CR_4IND"%s.serialize(enctyp, prev_image, out_buf);\n",
                                mdsc->get_member_name());
                    } else {
                        //class, struct  nmemb > 1
                        fprintf(file, CR_4IND"int alen = 0, alen_offst = out_buf.getPos();\n");
                        fprintf(file, CR_4IND"out_buf.mvPos(BlazeFramework.ARRAY_B_SZ);\n");
                        fprintf(file,
                                CR_4IND"alen = out_buf.getPos();  //we are reserving 4 bytes for array len.\n");
                        fprintf(file, CR_4IND"for(short i=0; i<%s.length; i++){\n",
                                mdsc->get_member_name());
                        fprintf(file, CR_5IND "if(!get%s()[i].isZero()){\n", mb_nm.internal_buff());
                        fprintf(file, CR_6IND"bb.putShort(i).flip();\n"
                                CR_6IND"out_buf.apndByteBuffer(bb);\n"
                                CR_6IND"bb.clear();\n");
                        fprintf(file, CR_6IND"%s[i].serialize(enctyp, prev_image, out_buf);\n",
                                mdsc->get_member_name());
                        fprintf(file, CR_5IND"}\n");
                        fprintf(file, CR_4IND"}\n");
                        fprintf(file, CR_4IND"bb.putInt(out_buf.getPos() - alen).flip();\n");
                        fprintf(file,
                                CR_4IND"out_buf.putByteBuffer(bb, alen_offst, BlazeFramework.ARRAY_B_SZ);\n"
                                CR_4IND"bb.clear();\n");
                    }
                }
            }
        } else if(mdsc->get_nmemb() == 1) {
            //primitive type nmemb == 1
            switch(mdsc->get_field_type()) {
                case Type_BOOL:
                    fprintf(file,
                            CR_4IND"bb.put(%s ? BlazeFramework.TRUE_BYTE : BlazeFramework.FALSE_BYTE).flip();\n"
                            CR_4IND"out_buf.apndByteBuffer(bb);\n"
                            CR_4IND"bb.clear();\n", mdsc->get_member_name());
                    break;
                case Type_INT16:
                case Type_UINT16:
                    fprintf(file, CR_4IND"bb.putShort(%s).flip();\n"
                            CR_4IND"out_buf.apndByteBuffer(bb);\n"
                            CR_4IND"bb.clear();\n", mdsc->get_member_name());
                    break;
                case Type_INT32:
                case Type_UINT32:
                    fprintf(file, CR_4IND"bb.putInt(%s).flip();\n"
                            CR_4IND"out_buf.apndByteBuffer(bb);\n"
                            CR_4IND"bb.clear();\n", mdsc->get_member_name());
                    break;
                case Type_INT64:
                case Type_UINT64:
                    fprintf(file, CR_4IND"bb.putLong(%s).flip();\n"
                            CR_4IND"out_buf.apndByteBuffer(bb);\n"
                            CR_4IND"bb.clear();\n", mdsc->get_member_name());
                    break;
                case Type_FLOAT32:
                    fprintf(file, CR_4IND"bb.putFloat(%s).flip();\n"
                            CR_4IND"out_buf.apndByteBuffer(bb);\n"
                            CR_4IND"bb.clear();\n", mdsc->get_member_name());
                    break;
                case Type_FLOAT64:
                    fprintf(file, CR_4IND"bb.putDouble(%s).flip();\n"
                            CR_4IND"out_buf.apndByteBuffer(bb);\n"
                            CR_4IND"bb.clear();\n", mdsc->get_member_name());
                    break;
                case Type_ASCII:
                    fprintf(file, CR_4IND"Short ascii_c = (short)%s.charValue();\n"
                            CR_4IND"bb.put(ascii_c.byteValue()).flip();\n"
                            CR_4IND"out_buf.apndByteBuffer(bb);\n"
                            CR_4IND"bb.clear();\n", mdsc->get_member_name());
                    break;
                default:
                    return blaze::RetCode_KO;
            }
        } else {
            //primitive type nmemb > 1
            if(isJavaStringType(mdsc)) {
                //strings
                switch(mdsc->get_field_type()) {
                    case Type_ASCII:
                        fprintf(file, CR_4IND"try{\n");
                        fprintf(file,
                                CR_5IND"ByteBuffer str_bb = ByteBuffer.wrap(%s.getBytes(\"US-ASCII\"));\n",
                                mdsc->get_member_name());
                        fprintf(file, CR_5IND"bb.putInt(str_bb.limit()).flip();\n");
                        fprintf(file, CR_5IND"out_buf.apndByteBuffer(bb);\n"
                                CR_5IND"bb.clear();\n");
                        fprintf(file, CR_5IND"out_buf.apndByteBuffer(str_bb);\n");
                        fprintf(file,
                                CR_4IND"}catch(UnsupportedEncodingException e){ e.printStackTrace(); }\n");
                        break;
                    default:
                        return blaze::RetCode_KO;
                }
            } else {
                //primitive type nmemb > 1
                blaze::ascii_string zero_val;
                RETURN_IF_NOT_OK(get_zero_val_for_BLZ_TYPE(mdsc->get_field_type(), zero_val))
                fprintf(file, CR_4IND"int alen = 0, alen_offst = out_buf.getPos();\n");
                fprintf(file, CR_4IND"out_buf.mvPos(BlazeFramework.ARRAY_B_SZ);\n");
                fprintf(file,
                        CR_4IND"alen = out_buf.getPos();  //we are reserving 4 bytes for array len.\n");
                fprintf(file, CR_4IND"for(short i=0; i<%s.length; i++){\n",
                        mdsc->get_member_name());
                fprintf(file, CR_5IND "if(get%s()[i] != %s){\n", mb_nm.internal_buff(),
                        zero_val.internal_buff());
                fprintf(file, CR_6IND"bb.putShort(i).flip();\n"
                        CR_6IND"out_buf.apndByteBuffer(bb);\n"
                        CR_6IND"bb.clear();\n");
                switch(mdsc->get_field_type()) {
                    case Type_BOOL:
                        fprintf(file,
                                CR_6IND"bb.put(%s[i] ? BlazeFramework.TRUE_BYTE : BlazeFramework.FALSE_BYTE).flip();\n"
                                CR_6IND"out_buf.apndByteBuffer(bb);\n"
                                CR_6IND"bb.clear();\n", mdsc->get_member_name());
                        break;
                    case Type_INT16:
                    case Type_UINT16:
                        fprintf(file, CR_6IND"bb.putShort(%s[i]).flip();\n"
                                CR_6IND"out_buf.apndByteBuffer(bb);\n"
                                CR_6IND"bb.clear();\n", mdsc->get_member_name());
                        break;
                    case Type_INT32:
                    case Type_UINT32:
                        fprintf(file, CR_6IND"bb.putInt(%s[i]).flip();\n"
                                CR_6IND"out_buf.apndByteBuffer(bb);\n"
                                CR_6IND"bb.clear();\n", mdsc->get_member_name());
                        break;
                    case Type_INT64:
                    case Type_UINT64:
                        fprintf(file, CR_6IND"bb.putLong(%s[i]).flip();\n"
                                CR_6IND"out_buf.apndByteBuffer(bb);\n"
                                CR_6IND"bb.clear();\n", mdsc->get_member_name());
                        break;
                    case Type_FLOAT32:
                        fprintf(file, CR_6IND"bb.putFloat(%s[i]).flip();\n"
                                CR_6IND"out_buf.apndByteBuffer(bb);\n"
                                CR_6IND"bb.clear();\n", mdsc->get_member_name());
                        break;
                    case Type_FLOAT64:
                        fprintf(file, CR_6IND"bb.putDouble(%s[i]).flip();\n"
                                CR_6IND"out_buf.apndByteBuffer(bb);\n"
                                CR_6IND"bb.clear();\n", mdsc->get_member_name());
                        break;
                    default:
                        return blaze::RetCode_KO;
                }
                fprintf(file, CR_5IND"}\n");
                fprintf(file, CR_4IND"}\n");
                fprintf(file, CR_4IND"bb.putInt(out_buf.getPos() - alen).flip();\n");
                fprintf(file,
                        CR_4IND"out_buf.putByteBuffer(bb, alen_offst, BlazeFramework.ARRAY_B_SZ);\n"
                        CR_4IND"bb.clear();\n");
            }
        }
        fprintf(file, CR_3IND "}\n");
    }
    fprintf(file, CR_3IND"bb.putInt(out_buf.getPos() - tlen).flip();\n");
    fprintf(file,
            CR_3IND"out_buf.putByteBuffer(bb, tlen_offst, BlazeFramework.ENTLN_B_SZ);\n");
    fprintf(file, CR_3IND"bb.clear();\n");
    return blaze::RetCode_OK;
}


/***********************************
RENDER- ABSR_METH_ENT_SER__Body_impl -*-SERIALIZE-*-
***********************************/
blaze::RetCode ABSR_METH_ENT_SER__Body_impl(compile_unit &cunit,
                                            entity_desc_comp &edsc,
                                            FILE *file)
{
    fprintf(file, CR_1IND"int tlen = 0;\n");
    fprintf(file, CR_1IND"switch(enctyp){\n");
    fprintf(file, CR_2IND"case BlazeFramework.BLZ_CLASS_ENCODE_IndexedNotZero:\n");
    RETURN_IF_NOT_OK(ABSR_METH_ENT_SER__IndexedNotZero(cunit, edsc, file))
    fprintf(file, CR_3IND"break;\n");
    fprintf(file, CR_2IND"default: return -1;\n");
    fprintf(file, CR_1IND"}\n");
    fprintf(file, CR_1IND"return out_buf.getPos();\n");
    return blaze::RetCode_OK;
}


/***********************************
RENDER- BLZ_COMP_JAVA_BLZ_Entity_abs_meths
***********************************/
blaze::RetCode BLZ_COMP_JAVA_BLZ_Entity_abs_meths(compile_unit &cunit,
                                                  entity_desc_comp &edsc,
                                                  FILE *file)
{
    fprintf(file,  "/*****************************************************\n"
            "BlazeEntity methods\n"
            "*****************************************************/\n");
    fprintf(file,  ABSR_METH_ENT_COPY"\n");
    RETURN_IF_NOT_OK(ABSR_METH_ENT_COPY__Body_impl(cunit, edsc, file))
    fprintf(file, "}\n");
    fprintf(file,  ABSR_METH_ENT_CLONE"\n");
    RETURN_IF_NOT_OK(ABSR_METH_ENT_CLONE__Body_impl(cunit, edsc, file))
    fprintf(file, "}\n");
    fprintf(file,  ABSR_METH_ENT_ISZERO"\n");
    RETURN_IF_NOT_OK(ABSR_METH_ENT_ISZERO__Body_impl(cunit, edsc, file))
    fprintf(file, "}\n");
    fprintf(file,  ABSR_METH_ENT_SETZERO"\n");
    RETURN_IF_NOT_OK(ABSR_METH_ENT_SETZERO__Body_impl(cunit, edsc, file))
    fprintf(file, "}\n");
    fprintf(file,  ABSR_METH_ENT_SET"\n");
    RETURN_IF_NOT_OK(ABSR_METH_ENT_SET__Body_impl(cunit, edsc, file))
    fprintf(file, "}\n");
    fprintf(file,  ABSR_METH_ENT_GCOMPVER"\n");
    RETURN_IF_NOT_OK(ABSR_METH_ENT_GCOMPVER__Body_impl(cunit, edsc, file))
    fprintf(file, "}\n");
    fprintf(file,  ABSR_METH_ENT_GENTDSC"\n");
    RETURN_IF_NOT_OK(ABSR_METH_ENT_GENTDSC__Body_impl(cunit, edsc, file))
    fprintf(file, "}\n");
    fprintf(file,  ABSR_METH_ENT_PTOBUF"\n");
    RETURN_IF_NOT_OK(ABSR_METH_ENT_PTOBUF__Body_impl(cunit, edsc, file))
    fprintf(file, "}\n");
    fprintf(file,  ABSR_METH_ENT_SER"\n");
    RETURN_IF_NOT_OK(ABSR_METH_ENT_SER__Body_impl(cunit, edsc, file))
    fprintf(file, "}\n");
    return blaze::RetCode_OK;
}

/***********************************
RENDER- BLZ_COMP_JAVA_BLZ_Class_abs_meths
***********************************/
blaze::RetCode BLZ_COMP_JAVA_BLZ_Class_abs_meths(compile_unit &cunit,
                                                 entity_desc_comp &edsc,
                                                 FILE *file)
{
    fprintf(file,  "/*****************************************************\n"
            "BlazeClass methods\n"
            "*****************************************************/\n");
    fprintf(file,  ABSR_METH_GETENTID"\n");
    fprintf(file,  CR_1IND"return %d;\n", edsc.get_entityid());
    fprintf(file, "}\n");
    return blaze::RetCode_OK;
}

/***********************************
RENDER- BLZ_COMP_JAVA_getter_setter
***********************************/
blaze::RetCode BLZ_COMP_JAVA_getter_setter(compile_unit &cunit,
                                           entity_desc_comp &edsc,
                                           FILE *file)
{
    fprintf(file,  "/*****************************************************\n"
            "getter / setter / iszero\n"
            "*****************************************************/\n");
    blaze::hash_map &entitymap = cunit.get_entity_map();
    blaze::hash_map &mmbr_map = edsc.get_map_id_MMBRDSC();
    mmbr_map.start_iteration();
    member_desc_comp *mdsc = NULL;
    while(!mmbr_map.next(NULL, &mdsc)) {
        if(mdsc->get_member_type() != MemberType_FIELD) {
            continue;
        }
        //getter
        blaze::ascii_string type_str;
        RETURN_IF_NOT_OK(target_type_from_BLZ_TYPE(*mdsc, entitymap, type_str))
        blaze::ascii_string meth_name;
        RETURN_IF_NOT_OK(meth_name.assign(BLZ_COMP_JAVA_GETTER_PFX))
        blaze::ascii_string cameled_fld_name;
        RETURN_IF_NOT_OK(cameled_fld_name.assign(mdsc->get_member_name()))
        cameled_fld_name.first_char_uppercase();
        RETURN_IF_NOT_OK(meth_name.append(cameled_fld_name))
        if(mdsc->get_field_type() == Type_ENTITY) {
            entity_desc_comp *inner_edsc = NULL;
            if(!entitymap.get(mdsc->get_field_usr_str_type(), &inner_edsc)) {
                if(inner_edsc->get_entity_type() == EntityType_ENUM) {
                    if(mdsc->get_nmemb() == 1) {
                        //enum  nmemb == 1
                        fprintf(file, "public %s ", type_str.internal_buff());
                    } else {
                        //enum  nmemb > 1
                        fprintf(file, "public %s[] ", type_str.internal_buff());
                    }
                } else {
                    if(mdsc->get_nmemb() == 1) {
                        //class, struct  nmemb == 1
                        fprintf(file, "public %s ", type_str.internal_buff());
                    } else {
                        //class, struct  nmemb > 1
                        fprintf(file, "public %s[] ", type_str.internal_buff());
                    }
                }
            }
        } else if(mdsc->get_nmemb() > 1) {
            //primitive type nmemb > 1
            fprintf(file, "public %s%s ", type_str.internal_buff(),
                    rec_brackets_cond(mdsc));
        } else {
            //primitive type nmemb == 1
            fprintf(file, "public %s ", type_str.internal_buff());
        }
        fprintf(file, "%s(){\n", meth_name.internal_buff());
        fprintf(file, CR_1IND"return %s;\n", mdsc->get_member_name());
        fprintf(file, "}\n");
        //setter
        RETURN_IF_NOT_OK(meth_name.assign(BLZ_COMP_JAVA_SETTER_PFX))
        RETURN_IF_NOT_OK(meth_name.append(cameled_fld_name))
        if(mdsc->get_field_type() == Type_ENTITY) {
            entity_desc_comp *edsc = NULL;
            if(!entitymap.get(mdsc->get_field_usr_str_type(), &edsc)) {
                if(edsc->get_entity_type() == EntityType_ENUM) {
                    if(mdsc->get_nmemb() == 1) {
                        //enum  nmemb == 1
                        fprintf(file, "public void %s(%s val){\n", meth_name.internal_buff(),
                                type_str.internal_buff());
                        fprintf(file, CR_1IND"%s = val;\n", mdsc->get_member_name());
                    } else {
                        //enum  nmemb > 1
                        fprintf(file, "public void %s(%s[] val){\n", meth_name.internal_buff(),
                                type_str.internal_buff());
                        fprintf(file, CR_1IND"for(int i=0; i<%s.length; i++){\n"
                                CR_2IND"%s[i].setValue(val[i].getValue());\n"
                                CR_1IND"}\n",   mdsc->get_member_name(),
                                mdsc->get_member_name());
                    }
                } else {
                    if(mdsc->get_nmemb() == 1) {
                        //class, struct  nmemb == 1
                        fprintf(file, "public void %s(%s val){\n", meth_name.internal_buff(),
                                type_str.internal_buff());
                        fprintf(file, CR_1IND"%s.set(val);\n", mdsc->get_member_name());
                    } else {
                        //class, struct  nmemb > 1
                        fprintf(file, "public void %s(%s[] val){\n", meth_name.internal_buff(),
                                type_str.internal_buff());
                        fprintf(file, CR_1IND"for(int i=0; i<%s.length; i++){\n"
                                CR_2IND"%s[i].set(val[i]);\n"
                                CR_1IND"}\n",   mdsc->get_member_name(),
                                mdsc->get_member_name());
                    }
                }
            }
        } else if(mdsc->get_nmemb() == 1) {
            //primitive type nmemb == 1
            fprintf(file, "public void %s(%s val){\n",  meth_name.internal_buff(),
                    type_str.internal_buff());
            fprintf(file, CR_1IND"%s = val;\n", mdsc->get_member_name());
        } else {
            //primitive type nmemb > 1
            if(isJavaStringType(mdsc)) {
                fprintf(file, "public void %s(%s val){\n", meth_name.internal_buff(),
                        type_str.internal_buff());
                fprintf(file,
                        CR_1IND"BlazeMemberDesc mdsc = getEntityDesc().getMmbrid_mdesc_Map().get((short)%d);\n",
                        mdsc->get_member_id());
                fprintf(file,
                        CR_1IND"%s = val.substring(0, Math.min(val.length(), mdsc.getNmemb()));\n",
                        mdsc->get_member_name());
            } else {
                blaze::ascii_string ttype;
                RETURN_IF_NOT_OK(target_type_from_builtin_BLZ_TYPE(*mdsc, ttype))
                //primitive type nmemb > 1
                fprintf(file, "public void %s(%s[] val){\n", meth_name.internal_buff(),
                        type_str.internal_buff());
                fprintf(file, CR_1IND"for(int i=0; i<%s.length; i++){\n"
                        CR_2IND"%s[i] = val[i];\n"
                        CR_1IND"}\n", mdsc->get_member_name(), mdsc->get_member_name());
            }
        }
        fprintf(file, "}\n");
        //zero method
        fprintf(file, "public boolean isZero%s(){\n", cameled_fld_name.internal_buff());
        blaze::ascii_string zero_val;
        if(mdsc->get_field_type() == Type_ENTITY) {
            entity_desc_comp *edsc = NULL;
            if(!entitymap.get(mdsc->get_field_usr_str_type(), &edsc)) {
                if(edsc->get_entity_type() == EntityType_ENUM) {
                    if(mdsc->get_nmemb() == 1) {
                        //enum  nmemb == 1
                        fprintf(file, CR_1IND"return %s.getValue() == 0;\n", mdsc->get_member_name());
                    } else {
                        //enum  nmemb > 1
                        fprintf(file, CR_1IND"for(BlazeEnum e : %s){\n"
                                CR_2IND"if(e.getValue() != 0) return false;\n"
                                CR_1IND"}\n"
                                CR_1IND"return true;\n", mdsc->get_member_name());
                    }
                } else {
                    //class, struct
                    if(mdsc->get_nmemb() == 1) {
                        //class, struct  nmemb == 1
                        fprintf(file, CR_1IND"return %s.isZero();\n", mdsc->get_member_name());
                    } else {
                        //class, struct  nmemb > 1
                        fprintf(file, CR_1IND"for(BlazeEntity e : %s){\n"
                                CR_2IND"if(!(e.isZero())) return false;\n"
                                CR_1IND"}\n"
                                CR_1IND"return true;\n", mdsc->get_member_name());
                    }
                }
            }
        } else if(mdsc->get_nmemb() == 1) {
            //primitive type nmemb == 1
            RETURN_IF_NOT_OK(get_zero_val_for_BLZ_TYPE(mdsc->get_field_type(), zero_val))
            fprintf(file, CR_1IND"return %s == %s;\n", mdsc->get_member_name(),
                    zero_val.internal_buff());
        } else {
            if(isJavaStringType(mdsc)) {
                fprintf(file, CR_1IND"return \"\".equals(%s);\n", mdsc->get_member_name());
            } else {
                blaze::ascii_string ttype;
                RETURN_IF_NOT_OK(target_type_from_builtin_BLZ_TYPE(*mdsc, ttype))
                RETURN_IF_NOT_OK(get_zero_val_for_BLZ_TYPE(mdsc->get_field_type(), zero_val))
                //primitive type nmemb > 1
                fprintf(file, CR_1IND"for(%s e : %s){\n"
                        CR_2IND"if(e != %s) return false;\n"
                        CR_1IND"}\n"
                        CR_1IND"return true;\n",ttype.internal_buff(), mdsc->get_member_name(),
                        zero_val.internal_buff());
            }
        }
        fprintf(file, "}\n");
    }
    return blaze::RetCode_OK;
}


/***********************************
RENDER- BLZ_COMP_JAVA_rep
***********************************/
blaze::RetCode BLZ_COMP_JAVA_rep(compile_unit &cunit,
                                 entity_desc_comp &edsc,
                                 FILE *file)
{
    fprintf(file,  "/*****************************************************\n"
            "rep.\n"
            "Fields have been declared public to efficiently perform\n"
            "BlazeEntity.restore() method.\n"
            "**USE GETTERS AND SETTERS TO ACCESS REP FIELDS.**\n"
            "*****************************************************/\n");
    blaze::hash_map &mmbr_map = edsc.get_map_id_MMBRDSC();
    member_desc_comp *mdsc = NULL;
    mmbr_map.start_iteration();
    //rep
    while(!mmbr_map.next(NULL, &mdsc)) {
        if(mdsc->get_member_type() != MemberType_FIELD) {
            continue;
        }
        blaze::ascii_string type_str;
        RETURN_IF_NOT_OK(target_type_from_BLZ_TYPE(*mdsc, cunit.get_entity_map(),
                                                   type_str))
        if(mdsc->get_nmemb() > 1) {
            //primitive type nmemb > 1
            fprintf(file, "public %s%s %s;", type_str.internal_buff(),
                    rec_brackets_cond(mdsc),  mdsc->get_member_name());
        } else {
            //primitive type nmemb == 1
            fprintf(file, "public %s %s;", type_str.internal_buff(),
                    mdsc->get_member_name());
        }
        RETURN_IF_NOT_OK(put_newline(file))
    }
    return blaze::RetCode_OK;
}

/***********************************
RENDER- BLZ_COMP_JAVA_package_decl
***********************************/
blaze::RetCode BLZ_COMP_JAVA_package_decl(FILE *file, const char *nmsp)
{
    fprintf(file, "package %s;", nmsp);
    return blaze::RetCode_OK;
}

/***********************************
RENDER- BLZ_COMP_JAVA_blz_import
***********************************/
blaze::RetCode BLZ_COMP_JAVA_blz_import(FILE *file)
{
    fprintf(file, "import java.io.*;\n");
    fprintf(file, "import java.nio.*;\n");
    fprintf(file, "import com.blaze.interf.*;\n");
    return blaze::RetCode_OK;
}

/***********************************
RENDER- BLZ_COMP_JAVA_BlzEnum
***********************************/
blaze::RetCode BLZ_COMP_JAVA_BlzEnum(compile_unit &cunit,
                                     entity_desc_comp &edsc,
                                     FILE *efile)
{
    fprintf(efile, "public class %s extends BlazeEnum{\n", edsc.get_entity_name());
    fprintf(efile, "/*****************************************************\n"
            "Enum values\n"
            "*****************************************************/\n");
    blaze::hash_map &mmbr_map = edsc.get_map_id_MMBRDSC();
    mmbr_map.start_iteration();
    member_desc_comp *mdsc = NULL;
    while(!mmbr_map.next(NULL, &mdsc)) {
        fprintf(efile, CR_1IND"public static final int %s = %ld;\n",
                mdsc->get_member_name(), mdsc->get_enum_value());
    }
    fprintf(efile, "}");
    return blaze::RetCode_OK;
}

/***********************************
RENDER- BLZ_COMP_JAVA_BlzClass
***********************************/
blaze::RetCode BLZ_COMP_JAVA_BlzClass(compile_unit &cunit,
                                      entity_desc_comp &edsc,
                                      FILE *efile)
{
    fprintf(efile, "public class %s extends BlazeClass{\n", edsc.get_entity_name());
    fprintf(efile,  "/*****************************************************\n"
            "CLASS ID\n"
            "*****************************************************/\n");
    fprintf(efile, "public static final int %s_CLASS_ID=%u;\n",
            edsc.get_entity_name(), edsc.get_entityid());
    RETURN_IF_NOT_OK(BLZ_COMP_JAVA_BLZ_Entity_Ctor(cunit, edsc, efile))
    RETURN_IF_NOT_OK(put_newline(efile))
    RETURN_IF_NOT_OK(BLZ_COMP_JAVA_BLZ_Entity_abs_meths(cunit, edsc, efile))
    RETURN_IF_NOT_OK(put_newline(efile))
    RETURN_IF_NOT_OK(BLZ_COMP_JAVA_BLZ_Class_abs_meths(cunit, edsc, efile))
    RETURN_IF_NOT_OK(put_newline(efile))
    RETURN_IF_NOT_OK(BLZ_COMP_JAVA_getter_setter(cunit, edsc, efile))
    RETURN_IF_NOT_OK(put_newline(efile))
    RETURN_IF_NOT_OK(BLZ_COMP_JAVA_rep(cunit, edsc, efile))
    RETURN_IF_NOT_OK(put_newline(efile))
    fprintf(efile, "}");
    return blaze::RetCode_OK;
}

/***********************************
RENDER- BLZ_COMP_JAVA_BlzStruct
***********************************/
blaze::RetCode BLZ_COMP_JAVA_BlzStruct(compile_unit &cunit,
                                       entity_desc_comp &edsc,
                                       FILE *efile)
{
    fprintf(efile, "public class %s extends BlazeStruct{\n",
            edsc.get_entity_name());
    RETURN_IF_NOT_OK(put_newline(efile))
    RETURN_IF_NOT_OK(BLZ_COMP_JAVA_BLZ_Entity_Ctor(cunit, edsc, efile))
    RETURN_IF_NOT_OK(put_newline(efile))
    RETURN_IF_NOT_OK(BLZ_COMP_JAVA_BLZ_Entity_abs_meths(cunit, edsc, efile))
    RETURN_IF_NOT_OK(put_newline(efile))
    RETURN_IF_NOT_OK(BLZ_COMP_JAVA_getter_setter(cunit, edsc, efile))
    RETURN_IF_NOT_OK(put_newline(efile))
    RETURN_IF_NOT_OK(BLZ_COMP_JAVA_rep(cunit, edsc, efile))
    RETURN_IF_NOT_OK(put_newline(efile))
    fprintf(efile, "}");
    return blaze::RetCode_OK;
}

/***********************************
RENDER- BLZ_COMP_JAVA_BlzEntityDesc
***********************************/
blaze::RetCode BLZ_COMP_JAVA_BlzEntityDesc(compile_unit &cunit,
                                           entity_desc_comp &edsc,
                                           FILE *file)
{
    fprintf(file,   CR_1IND"BlazeEntityDesc %s_edesc = new BlazeEntityDesc\n"
            CR_1IND"(\n"
            CR_2IND"%u, //int entityid\n"
            CR_2IND"%d, //int entitytype\n"
            CR_2IND"\"%s\", //String entity package\n"
            CR_2IND"\"%s\", //String entityname\n"
            CR_2IND"%u, //int fild_num\n"
            CR_2IND"%s //boolean persistent\n"
            CR_1IND");\n",
            edsc.get_entity_name(),
            edsc.get_entityid(),
            edsc.get_entity_type(),
            edsc.get_entity_namespace(),
            edsc.get_entity_name(),
            edsc.get_field_num(),
            edsc.is_persistent() ? "true" : "false");
    blaze::hash_map &mmbr_map = edsc.get_map_id_MMBRDSC();
    mmbr_map.start_iteration();
    member_desc_comp *mdsc = NULL;
    while(!mmbr_map.next(NULL, &mdsc)) {
        fprintf(file,   CR_1IND"BlazeMemberDesc %s_%s_mdesc = new BlazeMemberDesc\n"
                CR_1IND"(\n"
                CR_2IND"(short)%u, // short mmbrid\n"
                CR_2IND"%d, // int mmbr_type\n"
                CR_2IND"\"%s\", // String mmbr_name\n"
                CR_2IND"\"%s\", // String mmbr_desc\n"
                CR_2IND"%d, // int fild_type\n"
                CR_2IND"%lu, // int nmemb\n"
                CR_2IND"%u, // int fild_entityid\n"
                CR_2IND"\"%s\", // String fild_usr_str_type\n"
                CR_2IND"%d, // int fild_entitytype\n"
                CR_2IND"%ld  // int enum_value\n"
                CR_1IND");\n",
                edsc.get_entity_name(),
                mdsc->get_member_name(),
                mdsc->get_member_id(),
                mdsc->get_member_type(),
                mdsc->get_member_name(),
                mdsc->get_member_desc() ? mdsc->get_member_desc() : "",
                mdsc->get_field_type(),
                mdsc->get_nmemb(),
                mdsc->get_field_nclassid(),
                mdsc->get_field_usr_str_type() ? mdsc->get_field_usr_str_type() : "",
                mdsc->get_field_entity_type(),
                mdsc->get_enum_value());
        fprintf(file, CR_1IND"%s_edesc.addMemberDesc(%s_%s_mdesc);\n",
                edsc.get_entity_name(),
                edsc.get_entity_name(),
                mdsc->get_member_name());
    }
    if(edsc.is_persistent()) {
        blaze::hash_map &kdesc_map = edsc.get_map_keyid_KDESC_mod();
        kdesc_map.start_iteration();
        key_desc_comp *kdesc = NULL;
        while(!kdesc_map.next(NULL, &kdesc)) {
            fprintf(file,
                    CR_1IND"BlazeKeyDesc %s_%d_kdesc = new BlazeKeyDesc((short)%d, %s);\n",
                    edsc.get_entity_name(),
                    kdesc->get_key_id(),
                    kdesc->get_key_id(),
                    kdesc->is_primary() ? "true" : "false");
            member_desc_comp *k_mdsc = NULL;
            blaze::linked_list &kset = kdesc->get_key_member_set_m();
            kset.start_iteration();
            while(!kset.next(&k_mdsc)) {
                fprintf(file, CR_1IND"%s_%d_kdesc.addMemberDesc(%s_%s_mdesc);\n",
                        edsc.get_entity_name(),
                        kdesc->get_key_id(),
                        edsc.get_entity_name(),
                        k_mdsc->get_member_name());
            }
            fprintf(file, CR_1IND"%s_edesc.AddKeyDesc(%s_%d_kdesc);\n",
                    edsc.get_entity_name(),
                    edsc.get_entity_name(),
                    kdesc->get_key_id());
        }
    }
    return blaze::RetCode_OK;
}

extern int comp_ver[4];

/***********************************
RENDER- BLZ_COMP_Render_ModelVersion__Java_
***********************************/
blaze::RetCode BLZ_COMP_Render_ModelVersion__Java_(compile_unit &cunit,
                                                   FILE *file)
{
    fprintf(file,   "/*****************************************************\n"
            "MODEL:%s VERSION\n"
            "*****************************************************/\n", cunit.model_name());
    fprintf(file, "public static String get_MDL_Ver_%s()\n", cunit.model_name());
    fprintf(file, "{\n");
    fprintf(file,
            CR_1IND"return \"model.%s.ver.%s.compiler.ver.%d.%d.%d.%d.date:%s\";\n",
            cunit.model_name(),
            cunit.model_version(),
            comp_ver[0],
            comp_ver[1],
            comp_ver[2],
            comp_ver[3],
            __DATE__);
    fprintf(file, "}\n");
    return blaze::RetCode_OK;
}

/***********************************
RENDER- BLZ_COMP_JAVA_EntryPoint
***********************************/
blaze::RetCode BLZ_COMP_JAVA_EntryPoint(compile_unit &cunit,
                                        blaze::ascii_string &dfile_nm,
                                        FILE *file)
{
    blaze::ascii_string_tok tknz;
    blaze::ascii_string ep_name;
    RETURN_IF_NOT_OK(tknz.init(dfile_nm))
    RETURN_IF_NOT_OK(tknz.next_token(ep_name, BLZ_COMP_DOT))
    fprintf(file, "/*****************************************************\n"
            "bem entry point\n"
            "*****************************************************/\n");
    fprintf(file, "public final class %s{\n", ep_name.internal_buff());
    RETURN_IF_NOT_OK(put_newline(file))
    fprintf(file, "private static BlazeEntityManager bem;\n");
    RETURN_IF_NOT_OK(put_newline(file))
    fprintf(file, "public static BlazeEntityManager getBem(){\n");
    fprintf(file, CR_1IND"if(bem != null){\n"
            CR_2IND"return bem;\n"
            CR_1IND"}\n");
    fprintf(file, CR_1IND"bem = new BlazeEntityManager();\n");
    blaze::hash_map &entitymap = cunit.get_entity_map();
    entitymap.start_iteration();
    entity_desc_comp *edsc = NULL;
    while(!entitymap.next(NULL, &edsc)) {
        RETURN_IF_NOT_OK(BLZ_COMP_JAVA_BlzEntityDesc(cunit, *edsc, file))
        fprintf(file, CR_1IND"bem.extend(%s_edesc);\n", edsc->get_entity_name());
    }
    fprintf(file, CR_1IND"return bem;\n");
    fprintf(file, "}\n");
    RETURN_IF_NOT_OK(BLZ_COMP_Render_ModelVersion__Java_(cunit, file))
    fprintf(file, "}");
    return blaze::RetCode_OK;
}


/***********************************
ENTRYPOINT- BLZ_COMP_Compile_Java
***********************************/
blaze::RetCode compile_Java(compile_unit &cunit)
{
    blaze::hash_map &entitymap = cunit.get_entity_map();
    entitymap.start_iteration();
    entity_desc_comp *edsc = NULL;
    blaze::ascii_string efile_n;
    FILE *efile = NULL;
    //render entities
    while(!entitymap.next(NULL, &edsc)) {
        RETURN_IF_NOT_OK(efile_n.assign(edsc->get_entity_name()))
        RETURN_IF_NOT_OK(efile_n.append(".java"))
        COMMAND_IF_NOT_OK(open_output_file(efile_n.internal_buff(), &efile),
                          EXIT_ACTION("opening output file"))
        RETURN_IF_NOT_OK(render_hdr(cunit, efile_n, efile))
        RETURN_IF_NOT_OK(put_newline(efile))
        RETURN_IF_NOT_OK(BLZ_COMP_JAVA_package_decl(efile,
                                                    unit_nmspace.internal_buff()))
        RETURN_IF_NOT_OK(put_newline(efile))
        RETURN_IF_NOT_OK(BLZ_COMP_JAVA_blz_import(efile))
        RETURN_IF_NOT_OK(put_newline(efile))
        switch(edsc->get_entity_type()) {
            case EntityType_ENUM:
                RETURN_IF_NOT_OK(BLZ_COMP_JAVA_BlzEnum(cunit, *edsc, efile))
                break;
            case EntityType_NCLASS:
                RETURN_IF_NOT_OK(BLZ_COMP_JAVA_BlzClass(cunit, *edsc, efile))
                break;
            default:
                return blaze::RetCode_KO;
        }
        RETURN_IF_NOT_OK(put_newline(efile))
        fclose(efile);
    }
    //render entities descriptors
    blaze::ascii_string dfile_nm;
    dfile_nm.append(cunit.model_name());
    RETURN_IF_NOT_OK(dfile_nm.append(".java"));
    FILE *dfile = NULL;
    COMMAND_IF_NOT_OK(open_output_file(dfile_nm.internal_buff(), &dfile),
                      EXIT_ACTION("opening output file"))
    RETURN_IF_NOT_OK(render_hdr(cunit, dfile_nm, dfile))
    RETURN_IF_NOT_OK(put_newline(dfile))
    RETURN_IF_NOT_OK(BLZ_COMP_JAVA_package_decl(dfile,
                                                unit_nmspace.internal_buff()))
    RETURN_IF_NOT_OK(put_newline(dfile))
    RETURN_IF_NOT_OK(BLZ_COMP_JAVA_blz_import(dfile))
    RETURN_IF_NOT_OK(put_newline(dfile))
    RETURN_IF_NOT_OK(BLZ_COMP_JAVA_EntryPoint(cunit, dfile_nm, dfile))
    fclose(dfile);
    return blaze::RetCode_OK;
}

}