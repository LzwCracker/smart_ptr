share_ptr

✅ 完整的引用计数机制        
✅ 线程安全（原子操作）
✅ 自定义删除器支持         
✅ weak_ptr 完整实现  
✅ make_shared 性能优化     
✅ 别名构造函数
✅ 类型转换函数族            
✅ 异常安全保证
✅ 标准兼容的接口           
✅ 完善的比较运算符

Unique_ptr


class TestClass
{
public:
	std::string name;

	TestClass(const std::string& n) : name(n) 
	{
		std::cout << "TestClass " << name << " constructed\n";
	}

	~TestClass()
	{
		std::cout << "TestClass " << name << " destructed\n";
	}

	void sayHello() const
	{
		std::cout << "Hello from " << name << "\n";
	}
};


	// 测试基本类型
	{
		std::cout << "1. base   test:\n";
		auto ptr = utils::sp::make_unique<int>(42);
		std::cout << "Value: " << *ptr << "\n";
		*ptr = 100;
		std::cout << "Modified value: " << *ptr << std::endl;
	}

	// 测试数组版本
	{
		std::cout << "3. array test:\n";
		auto arrPtr = utils::sp::make_unique<int[]>(5);
		for (int i = 0; i < 5; ++i)
		{
			arrPtr[i] = (i + 1) * 10;
			std::cout << "arr[" << i << "] = " << arrPtr[i] << std::endl;
		}
		std::cout << std::endl;
	}


	// 测试移动语义
	{
		std::cout << "4. 移动语义测试:" << std::endl;
		auto ptr1 = utils::sp::make_unique<TestClass>("MovedObject");
		auto ptr2 = std::move(ptr1);

		if (!ptr1) 
		{
			std::cout << "ptr1 is now empty" << std::endl;
		}

		if (ptr2) 
		{
			std::cout << "ptr2 owns the object" << std::endl;
			ptr2->sayHello();
		}
		std::cout << std::endl;
	}

**share_ptr test**

class Base 
{
public:
	virtual ~Base() = default;
	virtual void print() const { std::cout << "Base\n"; }
};

class Derived : public Base
{
private:
	std::string name_;
public:
	Derived(const std::string& name) : name_(name) {}
	void print() const override 
	{
		std::cout << "Derived: " << name_ << std::endl;
	}
	const std::string& getName() const { return name_; }
};

   utils::sp::shared_ptr<int> p2= utils::sp::make_shared<int>(10);
	std::cout << "Value: " << *p2 << ", Use count: " << p2.use_count() << std::endl;
	{
		auto sp1 = p2;
		auto sp3 = sp1;
		std::cout << "After copy, sp1 use count: " << sp1.use_count() << std::endl;
		std::cout << "p2 use count: " << p2.use_count() << std::endl;
	}
	std::cout << "After scope, p2 use count: " << p2.use_count() << std::endl;

	int* raw_ptr = new int(100);
	auto del = [](int* p) 
	{
		std::cout << "Custom deleter called for: " << *p << std::endl;
		delete p;
	};
	utils::sp::shared_ptr<int> sp(raw_ptr, del);

	// 类型转换测试
	std::cout << "=== Type Cast Test ===" << std::endl;
	{
		auto derived_sp = utils::sp::make_shared<Derived>("TestObject");
		utils::sp::shared_ptr<Base> base_sp = derived_sp;
		base_sp->print();

		// 静态转换
		utils::sp::shared_ptr<Derived> casted_sp = utils::sp::static_pointer_cast<Derived>(base_sp);
		std::cout << "Static cast result: " << casted_sp->getName() << "\n";
	}

	// Weak pointer测试
	std::cout << "=== Weak Pointer Test ==="<< std::endl;
	{
		utils::sp::shared_ptr<int> sp = utils::sp::make_shared<int>(200);
		utils::sp::weak_ptr<int> wp(sp);

		std::cout << "sp use count: " << sp.use_count() << std::endl;
		std::cout << "Weak ptr use count: " << wp.use_count() << std::endl;
		std::cout << "Expired: " << (wp.expired() ? "yes" : "no") << std::endl;

		{
			auto locked = wp.lock();
			std::cout << "sp use count: " << sp.use_count() << std::endl;
			if (locked)
			{
				std::cout << "Locked value: " << *locked << std::endl;
			}
		}
		std::cout << "sp use count: " << sp.use_count() << std::endl;
		sp.reset();
		std::cout << "sp use count: " << sp.use_count() << std::endl;
		std::cout << "After shared ptr reset - Expired: " << (wp.expired() ? "yes" : "no") << std::endl;
		std::cout << "Weak ptr use count: " << wp.use_count() << std::endl;
		auto empty_lock = wp.lock();
		if (!empty_lock) 
		{
			std::cout << "Lock failed as expected"<< std::endl;
		}
	}
