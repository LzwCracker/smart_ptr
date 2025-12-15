
#pragma once

#include <atomic>
#include <functional>
#include <type_traits>

namespace utils
{
	namespace sp
	{
		class sp_counted_base
		{
		public:
			sp_counted_base();
			virtual ~sp_counted_base() = default;
			virtual void dispose() = 0;
			virtual void destroy() = 0;
		public:
			void add_ref_copy();
			void add_ref_lock();
			void weak_add_ref();
			void weak_release();
			void release();
			long use_count() const;
		protected:
		private:
			std::atomic<long> m_use_count;
			std::atomic<long> m_weak_count;
		};//sp_counted_base
//-----------------------------------------------------------------
			//sp_counted_impl_p：default deletor
		template<typename T>
		class sp_counted_impl_p :public sp_counted_base
		{
		public:
			explicit   sp_counted_impl_p(T* ptr) :m_ptr(ptr) {}
			virtual void dispose() override
			{
				delete m_ptr;
			}
			virtual void destroy() override
			{
				delete this;
			}
			T* get() const noexcept
			{
				return m_ptr;
			}
		private:
			T* m_ptr;
		};
		//----------------------------------------------------
		// define deletor
		template<typename T, typename D>
		class sp_counted_impl_pd : public sp_counted_base
		{
		public:
			sp_counted_impl_pd(T* p, D d) : m_ptr(p), deletor(d) {}

			virtual void dispose() override
			{
				deletor(m_ptr);
			}

			virtual void destroy() override
			{
				delete this;
			}

			T* get() const
			{
				return m_ptr;
			}
		private:
			T* m_ptr;
			D deletor;
		};
		//----------------------------------------------------------
		// Inline storage implementation（make_shared optimize）
		template<typename T>
		class sp_counted_impl_pdi : public sp_counted_base
		{
			//using storage_type = std::aligned_storage<sizeof(T), std::alignment_of<T>::value>::type;

		public:
			template<typename... Args>
			explicit sp_counted_impl_pdi(Args&&... args) : constructed_(true)
			{
                #undef new
				::new (static_cast<void*>(&storage_block)) T(std::forward<Args>(args)...);
				//new	(const_cast<void*>(static_cast<const volatile void*>(std::addressof(*storage_))))  T(std::forward<Args>(args)...);
				//new (&storage_) T(std::forward<Args>(args)...);
			}

			virtual void dispose() override
			{
				if (constructed_) 
				{
					get()->~T();
					constructed_ = false;
				}
			}

			virtual void destroy() override
			{
				delete this;
			}

			T* get() const noexcept
			{
				return const_cast<T*>(reinterpret_cast<T const*>(&storage_block));
			}
		private:
			typename std::aligned_storage<sizeof(T), std::alignment_of<T>::value>::type storage_block;
			bool constructed_;
		};

		namespace 
		{
			namespace detail
			{
				template<typename T> struct sp_element { typedef T type; };
				template<typename T> struct sp_element<T[]> { typedef T type; };
				template<typename T, std::size_t N> struct sp_element<T[N]> { typedef T type; };

				template<typename T> struct sp_dereference { typedef T& type; };
				template<> struct sp_dereference<void> { typedef void type; };
				template<> struct sp_dereference<void const> { typedef void type; };
				template<> struct sp_dereference<void volatile> { typedef void type; };
				template<> struct sp_dereference<void const volatile> { typedef void type; };

				//template <class T>
				//struct default_delete;

				template <class  T>
				struct default_delete // default deleter for unique_ptr 
				{ 
					constexpr default_delete() noexcept = default;

					template <class Tt, std::enable_if_t<std::is_convertible_v<Tt*, T*>, int> = 0>
					default_delete(const default_delete<Tt>&) noexcept {}

					void operator()(T* _Ptr) const noexcept /* strengthened */ 
					{ // delete a pointer
						static_assert(0 < sizeof(T), "can't delete an incomplete type");
						delete _Ptr;
					}
				};

				template <class T>
				struct default_delete<T[]>  // default deleter for unique_ptr to array of unknown size
				{ 
					constexpr default_delete() noexcept = default;

					template <class Tt, std::enable_if_t<std::is_convertible_v<Tt(*)[], T(*)[]>, int> = 0>
					default_delete(const default_delete<Tt[]>&) noexcept {}

					template <class Tt, std::enable_if_t<std::is_convertible_v<Tt(*)[], T(*)[]>, int> = 0>
					void operator()(Tt* _Ptr) const noexcept /* strengthened */ 
					{ // delete a pointer
						static_assert(0 < sizeof(Tt), "can't delete an incomplete type");
						delete[] _Ptr;
					}
				};

				template<typename T, typename D>
				class uniq_ptr_impl
				{
					template<typename U, typename V> friend class uniq_ptr_impl;
				private:
					union unique_ptr_data
					{
						T* m_ptr;
						typename std::aligned_storage<sizeof(D), std::alignment_of<D>::value>::type deleter_storage;
						constexpr unique_ptr_data(T* ptr) : m_ptr(ptr) {}
					} up_union_data;
				public:
					D& get_deleter() noexcept
					{
						return *reinterpret_cast<D*>(&up_union_data.deleter_storage);
					}

					const D& get_deleter() const noexcept
					{
						return *reinterpret_cast<const D*>(&up_union_data.deleter_storage);
					}
		
					T* get() const noexcept { return up_union_data.m_ptr; }

                    uniq_ptr_impl() noexcept : up_union_data(nullptr) {}

					explicit uniq_ptr_impl(T* ptr) : up_union_data(ptr) {}

					uniq_ptr_impl(T* ptr, D&& deleter) noexcept : up_union_data(ptr)
					{
						new (&up_union_data.deleter_storage) D(std::move(deleter));
					}

					uniq_ptr_impl(uniq_ptr_impl&& other) noexcept : up_union_data(other.up_union_data.m_ptr) 
					{
						if (other.get() != nullptr) 
						{
							new (&up_union_data.deleter_storage) D(std::move(other.get_deleter()));
						}
						other.up_union_data.m_ptr = nullptr;
					}

					template<typename U, typename V>
					uniq_ptr_impl(uniq_ptr_impl<U, V>&& other) noexcept  : up_union_data(other.up_union_data.m_ptr)
					{
						static_assert(!std::is_reference<V>::value || std::is_same<V, D>::value, "incompatible deleter");
						new (&up_union_data.deleter_storage) D(std::forward<V>(other.get_deleter()));
						other.up_union_data.m_ptr = nullptr;
					}

					~uniq_ptr_impl()
					{
						if (up_union_data.m_ptr)
						{
							get_deleter()(up_union_data.m_ptr);
						}
						if (up_union_data.m_ptr)
						{
                            get_deleter().~D();
						}
					}

					void reset(T* ptr) noexcept
					{
						T* old_ptr = up_union_data.m_ptr;
						up_union_data.m_ptr = ptr;
						if (old_ptr)
						{
							get_deleter()(old_ptr);
						}
					}

					T* release() noexcept
					{
						T* ptr = up_union_data.m_ptr;
						up_union_data.m_ptr = nullptr;
						return ptr;
					}
 
				};

				// Middle Layer - Provide type safety inspection and management functions
				template<typename T, typename D, bool =std:: is_move_constructible<D>::value,bool = std::is_move_assignable<D>::value>
				class uniq_ptr_data : private uniq_ptr_impl<T, D>
				{
					typedef uniq_ptr_impl<T, D> base_type;
				public:
					using base_type::base_type;
					using base_type::get;
					using base_type::get_deleter;
					using base_type::reset;
					using base_type::release;
					//uniq_ptr_data(uniq_ptr_data&&) = default;
					//uniq_ptr_data& operator=(uniq_ptr_data&&) = default;
				//	// constructor
				//	constexpr uniq_ptr_data() noexcept : base_type() {}

				//	explicit uniq_ptr_data(T* p) noexcept : base_type(p) {}

				//	// constructor with deleter
				//	uniq_ptr_data(T* p, const D& d) noexcept : base_type(p, const_cast<D&>(d)) {}

				//	// move constructor
				//	uniq_ptr_data(T* p, D&& d) noexcept : base_type(p, std::move(d)) {}
				};
			}// namespace detail
		}//empty namespace  

		template<typename T> class weak_ptr;
		template<typename T> class shared_ptr;

		template<typename T>
		class shared_ptr
		{
			typedef typename detail::sp_element<T>::type element_type;
			//using element_type = detail::sp_element<T>::type;
			template<typename Y> friend class shared_ptr;
			template<typename Y> friend class weak_ptr;
		public:
			using types=  typename shared_ptr<T>::element_type;
			constexpr shared_ptr() noexcept = default;

			constexpr shared_ptr(nullptr_t) noexcept {} // construct empty shared_ptr
			template<typename Y>
			explicit shared_ptr(Y* p) noexcept : px(p), pn(nullptr)
			{
				try
				{
					pn = new sp_counted_impl_p<Y>(p);
				}
				catch (...)
				{
					delete p;
					throw;
				}
			}

			template<typename Y, typename D>
			shared_ptr(Y* p, D d) noexcept  : px(p), pn(nullptr)
			{
				try
				{
					pn = new sp_counted_impl_pd<Y, D>(p, std::move(d));
				}
				catch (...)
				{
					d(p);
					throw;
				}
			}

			//copy constructor
			shared_ptr(shared_ptr const& r) noexcept : px(r.px), pn(r.pn)
			{
				if (pn != nullptr)
				{
					pn->add_ref_copy();
				}
			}

			template<typename Y>
			shared_ptr(shared_ptr<Y> const& r) noexcept : px(r.px), pn(r.pn)
			{
				if (pn != nullptr) {
					pn->add_ref_copy();
				}
			}

			// move constructor
			shared_ptr(shared_ptr&& r) noexcept  : px(r.px), pn(std::move(r).pn)
			{
				r.px = nullptr;
				r.pn = nullptr;
			}

			template<typename Y>
			shared_ptr(shared_ptr<Y>&& r) noexcept  : px(r.px), pn(std::move(r).pn)
			{
				r.px = nullptr;
				r.pn = nullptr;
			}

			// Alias constructor
			template<typename Y>
			shared_ptr(shared_ptr<Y> const& r, element_type* p) noexcept  : px(p), pn(r.pn)
			{
				if (pn != nullptr)
				{
					pn->add_ref_copy();
				}
			}

			template<typename Y>
			shared_ptr(shared_ptr<Y> const&& r, element_type* p) noexcept  : px(p), pn(std::move(r).pn)
			{
				if (pn != nullptr)
				{
					pn->add_ref_copy();
				}
			}
			// destructor
			~shared_ptr() noexcept
			{
				if (pn != nullptr)
				{
					pn->release();
				}
			}

			void swap(shared_ptr& other) noexcept
			{
				std::swap(px, other.px);
				std::swap(pn, other.pn);
			}

			// reset
			void reset() noexcept
			{
				shared_ptr().swap(*this);
			}

			template<typename Y>
			void reset(Y* p)
			{
				shared_ptr(p).swap(*this);
			}

			template<typename Y, typename D>
			void reset(Y* p, D d)
			{
				shared_ptr(p, d).swap(*this);
			}

			// observer
			typename detail::sp_dereference<T>::type operator*() const noexcept
			{
				return *px;
			}

			element_type* operator->() const  noexcept
			{
				return px;
			}

			element_type* get() const noexcept
			{
				return px;
			}

			long use_count() const noexcept
			{
				return pn ? pn->use_count() : 0;
			}

			bool unique() const noexcept
			{
				return use_count() == 1;
			}

			explicit operator bool() const noexcept
			{
				return px != nullptr;
			}

			// Assignment operation
			shared_ptr& operator=(shared_ptr const& r) noexcept
			{
				shared_ptr(r).swap(*this);
				return *this;
			}

			template<typename Y>
			shared_ptr& operator=(shared_ptr<Y> const& r) noexcept
			{
				shared_ptr(r).swap(*this);
				return *this;
			}

			shared_ptr& operator=(shared_ptr&& r) noexcept
			{
				shared_ptr(std::move(r)).swap(*this);
				return *this;
			}

			template<typename Y>
			shared_ptr& operator=(shared_ptr<Y>&& r) noexcept
			{
				shared_ptr(std::move(r)).swap(*this);
				return *this;
			}
		private:
			template<typename T, typename... Args>
			friend shared_ptr<T> make_shared(Args&&... _Args)noexcept(std::is_nothrow_constructible_v<T, Args...>);
			void  set_ptr_rep(element_type* px, sp_counted_base* p)
			{
				this->px = px;
				this->pn = p;
			}
		private:
			element_type* px;
			sp_counted_base* pn;

		};

		// comparison operator
		template<typename T, typename U>
		inline bool operator==(shared_ptr<T> const& Lv, shared_ptr<U> const& Rv) noexcept
		{
			return Lv.get() == Rv.get();
		}

		template<typename T, typename U>
		inline bool operator!=(shared_ptr<T> const& Lv, shared_ptr<U> const& Rv) noexcept
		{
			return  Lv.get() != Rv.get();
		}

		template<typename T, typename U>
		inline bool operator<(shared_ptr<T> const& Lv, shared_ptr<U> const& Rv) noexcept
		{
			return std::less<typename std::common_type<T*, U*>::type>()(Lv.get(), Rv.get());
		}

		template<typename T, typename U>
		inline bool operator<=(shared_ptr<T> const& Lv, shared_ptr<U> const& Rv) noexcept
		{
			return !(Rv.get() < Lv.get());
		}

		template<typename T, typename U>
		inline bool operator>(shared_ptr<T> const& Lv, shared_ptr<U> const& Rv) noexcept
		{
			return Rv.get() < Lv.get();
		}

		template<typename T, typename U>
		inline bool operator>=(shared_ptr<T> const& Lv, shared_ptr<U> const& Rv) noexcept
		{
			return !(Lv.get() < Rv.get());
		}

		// A family of type conversion functions
		template<typename T, typename U>
		shared_ptr<T> static_pointer_cast(shared_ptr<U> const& r) noexcept
		{
			(void) static_cast<T*>(static_cast<U*>(0));
			typedef typename shared_ptr<T>::types E;
			E* p = static_cast<E*>(r.get());
			return shared_ptr<T>(r, p);
		}

		template<typename T, typename U>
		shared_ptr<T> dynamic_pointer_cast(shared_ptr<U> const& r) noexcept
		{
			(void) dynamic_cast<T*>(static_cast<U*>(0));
			typedef typename shared_ptr<T>::types E;
			E* p = dynamic_cast<E*>(r.get());
			return p ? shared_ptr<T>(r, p) : shared_ptr<T>();
		}

		template<typename T, typename U>
		shared_ptr<T> const_pointer_cast(shared_ptr<U> const& r) noexcept
		{
			(void) const_cast<T*>(static_cast<U*>(0));
			typedef typename shared_ptr<T>::types E;
			E* p = const_cast<E*>(r.get());
			return shared_ptr<T>(r, p);
		}

		//make_shared

		template<typename T, typename... Args>
		shared_ptr<T> make_shared(Args&&... args)noexcept(std::is_nothrow_constructible_v<T, Args...>)
		{
			typedef typename   std::remove_cv<T>::type  T_ncv;
			sp_counted_impl_pdi<T_ncv>* pi = new sp_counted_impl_pdi<T_ncv>(std::forward<Args>(args)...);
			shared_ptr<T> Ret;
			Ret.set_ptr_rep(pi->get(), pi);
			return Ret;
		}

//-------------------weak_ptr---------------------------------
		template<typename T>
		class weak_ptr
		{
			template<typename Y> friend class weak_ptr;
			template<typename Y> friend class shared_ptr;
		public:
			// constructor
			constexpr weak_ptr() : px(nullptr), pn(nullptr) {}

			template<typename Y>
			weak_ptr(shared_ptr<Y> const& r) noexcept  : px(r.px), pn(r.pn)
			{
				if (pn != nullptr) 
				{
					pn->weak_add_ref();
				}
			}

			// copy constructor
			weak_ptr(weak_ptr const& r) noexcept : px(r.px), pn(r.pn)
			{
				if (pn != nullptr) 
				{
					pn->weak_add_ref();
				}
			}

			template<typename Y>
			weak_ptr(weak_ptr<Y> const& r) noexcept : px(r.lock().get()), pn(r.pn)
			{
				if (pn != nullptr) 
				{
					pn->weak_add_ref();
				}
			}

			// move constructor
			weak_ptr(weak_ptr&& r) noexcept  : px(r.px), pn(r.pn)
			{
				r.px = nullptr;
				r.pn = nullptr;
			}

			template<typename Y>
			weak_ptr(weak_ptr<Y>&& r) noexcept  : px(r.px), pn(r.pn)
			{
				r.px = nullptr;
				r.pn = nullptr;
			}

			// Assignment operation
			weak_ptr& operator=(weak_ptr const& r) noexcept
			{
				weak_ptr(r).swap(*this);
				return *this;
			}

			template<typename Y>
			weak_ptr& operator=(weak_ptr<Y> const& r) noexcept
			{
				weak_ptr(r).swap(*this);
				return *this;
			}

			template<typename Y>
			weak_ptr& operator=(shared_ptr<Y> const& r) noexcept
			{
				weak_ptr(r).swap(*this);
				return *this;
			}

			weak_ptr& operator=(weak_ptr&& r)  noexcept
			{
				weak_ptr(std::move(r)).swap(*this);
				return *this;
			}

			template<typename Y>
			weak_ptr& operator=(weak_ptr<Y>&& r) noexcept
			{
				weak_ptr(std::move(r)).swap(*this);
				return *this;
			}

			// destrouctor
			~weak_ptr() noexcept
			{
				if (pn != nullptr) 
				{
					pn->weak_release();
				}
			}

			// reset
			void reset() noexcept
			{
				weak_ptr().swap(*this);
			}

			// swap
			void swap(weak_ptr& other) noexcept
			{
				std::swap(px, other.px);
				std::swap(pn, other.pn);
			}

			// observer
			long use_count() const noexcept
			{
				return pn ? pn->use_count() : 0;
			}

			bool expired() const noexcept 
			{
				return use_count() == 0;
			}

			shared_ptr<T> lock() const noexcept
			{
				if (expired()) 
				{
					return shared_ptr<T>();
				}

				shared_ptr<T> p;
				p.px = px;
				p.pn = pn;
				if (pn)
				{
					pn->add_ref_lock();
				}
				return p;
			}
		private:
			typedef typename detail::sp_element<T>::type element_type;
			element_type* px;
			sp_counted_base* pn;
		};



		template <class T, class D=detail::default_delete<T>>
		class unique_ptr;

		template <class T, class D /* = default_delete<T> */>
		class unique_ptr : private detail::uniq_ptr_data <T, D>
		{
			
		public:
			typedef T* pointer;
			//typedef T element_type;
			typedef D deleter_type;

			// constructor
			constexpr unique_ptr() noexcept : data_type(pointer()) {}

			explicit unique_ptr(pointer p) noexcept :data_type(p)
			{
				//using del = std::conditional<std::is_reference<D>::value, D, const D&>;
				//del d;
				//data_type(p, d);
			}

			unique_ptr(pointer p, typename std::conditional<std::is_reference<D>::value, D, const D&>::type d) noexcept : data_type(p, d) {}

			unique_ptr(pointer p, typename std::remove_reference<D>::type&& d) noexcept : data_type(p, std::move(d))
			{
				static_assert(!std::is_reference<D>::value, "rvalue deleter bound to reference");
			}

			// move constructor
			unique_ptr(unique_ptr&& u) noexcept : data_type(std::move(u)) {}

			template<typename U, typename E>
			unique_ptr(unique_ptr<U, E>&& u) noexcept : data_type(std::move(u)) {}

			// disabled copy
			unique_ptr(const unique_ptr&) = delete;
			unique_ptr& operator=(const unique_ptr&) = delete;

			// destructor
			~unique_ptr() = default;

			// mov alignment operator
			unique_ptr& operator=(unique_ptr&& u) noexcept {
				data_type::reset(u.release());
				data_type::get_deleter() = std::forward<D>(u.get_deleter());
				return *this;
			}

			template<typename U, typename E>
			unique_ptr& operator=(unique_ptr<U, E>&& u) noexcept
			{
				data_type::reset(u.release());
				data_type::get_deleter() = std::forward<E>(u.get_deleter());
				return *this;
			}

			// pointer
			typename std::add_lvalue_reference<T>::type operator*() const
			{
				return *get();
			}

			pointer operator->() const noexcept
			{
				return get();
			}

			pointer get() const noexcept {
				return data_type::get();
			}

			deleter_type& get_deleter() noexcept {
				return data_type::get_deleter();
			}

			const deleter_type& get_deleter() const noexcept 
			{
				return data_type::get_deleter();
			}

			explicit operator bool() const noexcept 
			{
				return get() != pointer();
			}

			pointer release() noexcept
			{
				return data_type::release();
			}

			void reset(pointer p = pointer()) noexcept 
			{
				data_type::reset(p);
			}

			void swap(unique_ptr& u) noexcept 
			{
				data_type::swap(u.data_type);
			}
		private:
			typedef detail::uniq_ptr_data<T, D> data_type;
		};
 
		template <class T, class D>
		class unique_ptr<T[], D>:private  detail::uniq_ptr_data<T, D>
		{
		public:
			typedef T* pointer;
			//typedef T element_type;
			typedef D deleter_type;

			// constructor
			constexpr unique_ptr() noexcept : data_type(pointer()) {}

			explicit unique_ptr(pointer p) noexcept : data_type(p) {}

			unique_ptr(pointer p, typename std::conditional<std::is_reference<D>::value, D, const D&>::type d) noexcept: data_type(p, d) {}

			unique_ptr(pointer p, typename std::remove_reference<D>::type&& d) noexcept: data_type(p, std::move(d)) 
			{
				static_assert(!std::is_reference<D>::value, "rvalue deleter bound to reference");
			}

			// move constructor
			unique_ptr(unique_ptr&& u) noexcept : __data_type(std::move(u)) {}

			// disabled copy
			unique_ptr(const unique_ptr&) = delete;
			unique_ptr& operator=(const unique_ptr&) = delete;

			// deconstructor
			~unique_ptr() = default;

			// move alignment operator
			unique_ptr& operator=(unique_ptr && u) noexcept {
				data_type::reset(u.release());
				data_type::get_deleter() = std::forward<D>(u.get_deleter());
				return *this;
			}

			// array access operator
			T& operator[](size_t i) const
			{
				return get()[i];
			}

			pointer get() const noexcept
			{
				return data_type::get();
			}

			deleter_type& get_deleter() noexcept
			{
				return data_type::get_deleter();
			}

			const deleter_type& get_deleter() const noexcept 
			{
				return data_type::get_deleter();
			}

			explicit operator bool() const noexcept 
			{
				return get() != pointer();
			}

			pointer release() noexcept
			{
				return data_type::release();
			}

			void reset(pointer p = pointer()) noexcept
			{
				data_type::reset(p);
			}
		private:
			typedef detail::uniq_ptr_data<T, D> data_type;
		};

		// comparison operator
		template<typename T, typename D>
		inline bool operator==(const unique_ptr<T, D>& x, const unique_ptr<T, D>& y)
		{
			return x.get() == y.get();
		}

		template<typename T, typename D>
		inline bool operator!=(const unique_ptr<T, D>& x, const unique_ptr<T, D>& y) 
		{
			return x.get() != y.get();
		}

		template<typename T, typename D>
		inline bool operator<(const unique_ptr<T, D>& x, const unique_ptr<T, D>& y) 
		{
			return x.get() < y.get();
		}

		template<typename T, typename D>
		inline bool operator<=(const unique_ptr<T, D>& x, const unique_ptr<T, D>& y) 
		{
			return x.get() <= y.get();
		}

		template<typename T, typename D>
		inline bool operator>(const unique_ptr<T, D>& x, const unique_ptr<T, D>& y)
		{
			return x.get() > y.get();
		}

		template<typename T, typename D>
		inline bool operator>=(const unique_ptr<T, D>& x, const unique_ptr<T, D>& y)
		{
			return x.get() >= y.get();
		}

		// compare nullptr_t
		template<typename T, typename D>
		inline bool operator==(const unique_ptr<T, D>& x, std::nullptr_t) noexcept 
		{
			return !x;
		}

		template<typename T, typename D>
		inline bool operator==(std::nullptr_t, const unique_ptr<T, D>& x) noexcept 
		{
			return !x;
		}

		template<typename T, typename D>
		inline bool operator!=(const unique_ptr<T, D>& x, std::nullptr_t) noexcept 
		{
			return static_cast<bool>(x);
		}

		template<typename T, typename D>
		inline bool operator!=(std::nullptr_t, const unique_ptr<T, D>& x) noexcept 
		{
			return static_cast<bool>(x);
		}

		//make_unique
		template <class T, class... Args, std::enable_if_t<!std::is_array_v<T>, int> = 0>
		inline unique_ptr<T> make_unique(Args&&... _Args)
		{
			return unique_ptr<T>(new T(std::forward<Args>(_Args)...));
		}

		template <class T, std::enable_if_t<std::is_array_v<T>&& std::extent_v<T> == 0, int> = 0>
		inline unique_ptr<T> make_unique(const size_t nSize)
		{
			using _Elem = std::remove_extent_t<T>;
			return unique_ptr<T>(new _Elem[nSize]());
		}

		//template<typename T, typename... Args>
		//inline unique_ptr<T> make_unique(Args&&... args) 
		//{
		//	return unique_ptr<T>(new T(std::forward<Args>(args)...));
		//}

		//template<typename T>
		//inline unique_ptr<T> make_unique(std::size_t size) 
		//{
		//	return unique_ptr<T>(new typename std::remove_extent<T>::type[size]());
		//}
	}//  namespace sp
}//namespace utils


