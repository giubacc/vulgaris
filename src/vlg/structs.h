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

#ifndef CR_STRUCTS_MT_H_
#define CR_STRUCTS_MT_H_
#include "vlg.h"
#define HAVE_STRUCT_TIMESPEC
#include <pthread.h>

namespace vlg {

typedef void *(*alloc_func)(size_t type_size,
                            const void *copy);

typedef void(*dealloc_func)(void *ptr);

typedef void *(*cpy_func)(void *copy,
                          const void *ptr,
                          size_t type_size);

typedef int(*cmp_func)(const void *obj1,
                       const void *obj2,
                       size_t len);

typedef void(*hash_func)(const void *key,
                         int len,
                         uint32_t seed,
                         void *out);

/** @brief Object Manager class.
*/
struct obj_mng {
    explicit obj_mng(size_t type_size);

    explicit obj_mng(const obj_mng &other);

    explicit obj_mng(size_t type_size,
                     alloc_func alloc_f,
                     dealloc_func dealloc_f,
                     cmp_func cmp_f,
                     cpy_func copy_f,
                     hash_func hash_f);

    void *new_obj(const void *copy) const {
        return alloc_func_(type_size_, copy);
    }

    void del_obj(void *ptr) const {
        dealloc_func_(ptr);
    }

    void *copy_obj(void *copy,
                   const void *ptr) const {
        return copy ? cpy_func_(copy, ptr, type_size_) : nullptr;
    }

    int cmp_obj(const void *ptr1,
                const void *ptr2) const {
        return cmp_func_(ptr1, ptr2, type_size_);
    }

    void hash_obj(const void *ptr,
                  void *out) const {
        hash_func_(ptr, (int)type_size_, seed_, out);
    }

    uint32_t seed_;
    size_t type_size_;
    alloc_func alloc_func_;
    dealloc_func dealloc_func_;
    cmp_func cmp_func_;
    cpy_func cpy_func_;
    hash_func hash_func_;
};

/** @brief Pointer Object Manager class.
*/
class ptr_obj_mng : public obj_mng {
        friend const ptr_obj_mng &sngl_ptr_obj_mng();
    public:

        ptr_obj_mng();

    private:
        static ptr_obj_mng *instance_;
};

/** @brief c-string Object Manager class.
*/
class cstr_obj_mng : public obj_mng {
        friend const cstr_obj_mng &sngl_cstr_obj_mng();
    public:

        cstr_obj_mng();

    private:
        static cstr_obj_mng *instance_;
};

const ptr_obj_mng &sngl_ptr_obj_mng();
const cstr_obj_mng &sngl_cstr_obj_mng();

}

namespace vlg {

struct scoped_rd_lock {
    explicit scoped_rd_lock(pthread_rwlock_t &lock) : lock_(lock) {
        pthread_rwlock_rdlock(&lock_);
    }
    ~scoped_rd_lock() {
        pthread_rwlock_unlock(&lock_);
    }
    pthread_rwlock_t &lock_;
};

struct scoped_wr_lock {
    explicit scoped_wr_lock(pthread_rwlock_t &lock) : lock_(lock) {
        pthread_rwlock_wrlock(&lock_);
    }
    ~scoped_wr_lock() {
        pthread_rwlock_unlock(&lock_);
    }
    pthread_rwlock_t &lock_;
};

/** @brief mx class.
*/
class mx {
    public:

        explicit mx(int pshared = PTHREAD_PROCESS_PRIVATE);
        ~mx();

        int lock() {
            return pthread_mutex_lock(&mutex_);
        }

        int unlock() {
            return pthread_mutex_unlock(&mutex_);
        }

        int wait() {
            return pthread_cond_wait(&cv_, &mutex_);
        }

        int wait(time_t sec,
                 long nsec);

        int notify() {
            return pthread_cond_signal(&cv_);
        }

        int notify_all() {
            return pthread_cond_broadcast(&cv_);
        }

    private:
        pthread_mutexattr_t mattr_;
        pthread_mutex_t mutex_;
        pthread_condattr_t cattr_;
        pthread_cond_t cv_;
};

struct scoped_mx {
    scoped_mx(mx &mx) : mx_(mx) {
        mx_.lock();
    }
    ~scoped_mx() {
        mx_.unlock();
    }
    mx &mx_;
};

}

namespace vlg {

struct brep {
        brep(): elemcount_(0) {}
        void dcr_elems() {
            elemcount_--;
        }
        void incr_elems() {
            elemcount_++;
        }
        void rst_elems() {
            elemcount_ = 0;
        }
        uint32_t elems_cnt()  const {
            return elemcount_;
        }

    private:
        uint32_t elemcount_;
};

class s_hm;

typedef void(*s_hm_enm_func)(const s_hm &map,
                             const void *key,
                             const void *ptr,
                             void *ud);

typedef void(*s_hm_enm_func_br)(const s_hm &map,
                                const void *key,
                                const void *ptr,
                                void *ud,
                                bool &brk);

const unsigned int HMSz_23 = 23U;
const unsigned int HMSz_71 = 71U;
const unsigned int HMSz_101 = 101U;
const unsigned int HMSz_1031 = 1031U;
const unsigned int HMSz_10037 = 10037U;
const unsigned int HMSz_33013 = 33013U;
const unsigned int HMSz_65557 = 65557U;
const unsigned int HMSz_130343 = 130343U;
const unsigned int HMSz_257177 = 257177U;
const unsigned int HMSz_517411 = 517411U;
const unsigned int HMSz_1031117 = 1031117U;

/** @brief Basic thread safe Hash-Map class.
*/
struct hm_node;
class s_hm : public brep {
    public:

        explicit s_hm(uint32_t hash_size,
                      size_t elem_size,
                      size_t key_size,
                      pthread_rwlockattr_t *attr = nullptr);

        explicit s_hm(uint32_t hash_size,
                      const obj_mng &elem_manager,
                      const obj_mng &key_manager,
                      pthread_rwlockattr_t *attr = nullptr);

        explicit s_hm(uint32_t hash_size,
                      size_t elem_size,
                      const obj_mng &key_manager,
                      pthread_rwlockattr_t *attr = nullptr);

        explicit s_hm(uint32_t hash_size,
                      const obj_mng &elem_manager,
                      size_t key_size,
                      pthread_rwlockattr_t *attr = nullptr);

        ~s_hm();

    public:

        RetCode clear();

        /**
        @param key
        @param copy
        @return RetCode_OK if the specified key is mapped,
        RetCode_KO if this map contains no mapping for the key or
        RetCode_BDARG if key is nullptr.
        */
        RetCode contains_key(const void *key) const;

        RetCode enum_elements_safe_read(s_hm_enm_func enum_f,
                                        void *ud) const;

        RetCode enum_elements_safe_write(s_hm_enm_func enum_f,
                                         void *ud) const;

        RetCode enum_elements_breakable_safe_read(s_hm_enm_func_br enum_f,
                                                  void *ud) const;

        RetCode enum_elements_breakable_safe_write(s_hm_enm_func_br
                                                   enum_f,
                                                   void *ud) const;
        /**
        @param key
        @param copy
        @return RetCode_OK if the specified key is mapped,
        RetCode_KO if this map contains no mapping for the key, or
        RetCode_BDARG if key is nullptr.
        */
        RetCode get(const void *key,
                    void *copy) const;

        bool is_empty() const {
            return elems_cnt() == 0;
        }

        RetCode remove(const void *key,
                       void *copy);

        RetCode put(const void *key,
                    const void *ptr);

        uint32_t size() const {
            return elems_cnt();
        }

    private:
        void init(pthread_rwlockattr_t *attr);
        uint32_t gidx(const void *key) const;
        void rm(hm_node *del_mn, uint32_t idx);
        void enm(const s_hm &map, s_hm_enm_func enum_f, void *ud) const;
        void enmbr(const s_hm &map, s_hm_enm_func_br enum_f, void *ud) const;

    private:
        const obj_mng elem_manager_, key_manager_;
        uint32_t hash_size_;
        hm_node **buckets_;
        hm_node  *head_, *tail_, *it_, *prev_;
        mutable pthread_rwlock_t lock_;
};

/** @brief Basic Blocking-Queue class.
*/
struct lnk_node;
class b_qu : public brep {
    public:

        explicit b_qu(size_t elemsize,
                      uint32_t capacity = 0);

        explicit b_qu(const obj_mng &elem_manager,
                      uint32_t capacity = 0);

        ~b_qu();

    public:

        bool is_empty() const {
            return elems_cnt() == 0;
        }

        uint32_t size() const {
            return elems_cnt();
        }

        uint32_t remain_capacity() const {
            return inifinite_cpcty_ ? 1 : (capacity_ - elems_cnt());
        }

        RetCode clear();

        /**
        waits indefinitely until an element becomes available.
        @param copy
        @return RetCode_OK when an element becomes available.
        */
        RetCode get(void *copy);

        /**
        same as above, except that this method waits at most for sec/nsec seconds
        before return with RetCode_TMOUT code.
        Use sec=0 and nsec=0 if you want this method to return immediatly
        with rescode RetCode_EPTY if no elements are available.
        @param sec
        @param nsec
        @param copy
        @return RetCode_OK when an element becomes available. RetCode_TMOUT if no
        elements became available in sec/nsec after invocation. RetCode_EPTY if
        no elements are available and sec=0 and nsec=0.
        */
        RetCode get(time_t sec,
                    long nsec,
                    void *copy);

        RetCode put(const void *ptr);

        RetCode put(time_t sec,
                    long nsec,
                    const void *ptr);

        RetCode peek(void *copy);

        RetCode peek(time_t sec,
                     long nsec,
                     void *copy);

    private:
        void dq(void *copy);
        RetCode enq(const void *ptr);

    private:
        obj_mng manager_;
        bool inifinite_cpcty_;
        uint32_t capacity_;
        lnk_node *head_;
        lnk_node *tail_;
        uint32_t wt_prod_, wt_cons_;
        mutable mx mon_;
};

/** @brief object manager for std shared pointers.
*/
template <typename T>
class std_shared_ptr_obj_mng : public obj_mng {
    private:
        static void *shared_ptr_alloc_func(size_t type_size,
                                           const void *copy) {
            std::shared_ptr<T> &sh_ptr_cpy = *(std::shared_ptr<T> *)(copy);
            std::shared_ptr<T> *new_sh_ptr = new std::shared_ptr<T>(sh_ptr_cpy);
            return new_sh_ptr;
        }

        static void shared_ptr_dealloc_func(void *ptr) {
            std::shared_ptr<T> *sh_ptr = (std::shared_ptr<T> *)(ptr);
            delete sh_ptr;
        }

        static void *shared_ptr_cpy_func(void *copy,
                                         const void *ptr,
                                         size_t type_size) {
            std::shared_ptr<T> &sh_ptr_cpy = *(std::shared_ptr<T> *)(copy);
            std::shared_ptr<T> &sh_ptr_ptr = *(std::shared_ptr<T> *)(ptr);
            sh_ptr_cpy = sh_ptr_ptr;
            return copy;
        }

    public:
        std_shared_ptr_obj_mng() : obj_mng(0,
                                               shared_ptr_alloc_func,
                                               shared_ptr_dealloc_func,
                                               0,
                                               shared_ptr_cpy_func,
                                               0) {}
};

/** @brief object manager for std unique pointers.
*/
template <typename T>
class std_unique_ptr_obj_mng : public obj_mng {
    private:
        static void *unique_ptr_alloc_func(size_t type_size,
                                           const void *copy) {
            std::unique_ptr<T> &uq_ptr_cpy = *(std::unique_ptr<T> *)(copy);
            std::unique_ptr<T> *new_uq_ptr = new std::unique_ptr<T>(std::move(uq_ptr_cpy));
            return new_uq_ptr;
        }

        static void unique_ptr_dealloc_func(void *ptr) {
            std::unique_ptr<T> *uq_ptr = (std::unique_ptr<T> *)(ptr);
            delete uq_ptr;
        }

        static void *unique_ptr_cpy_func(void *copy,
                                         const void *ptr,
                                         size_t type_size) {
            std::unique_ptr<T> &uq_ptr_cpy = *(std::unique_ptr<T> *)(copy);
            std::unique_ptr<T> &uq_ptr_ptr = *(std::unique_ptr<T> *)(ptr);
            uq_ptr_cpy = std::move(uq_ptr_ptr);
            return copy;
        }

    public:
        std_unique_ptr_obj_mng() : obj_mng(0,
                                               unique_ptr_alloc_func,
                                               unique_ptr_dealloc_func,
                                               0,
                                               unique_ptr_cpy_func,
                                               0) {}
};

}

#endif