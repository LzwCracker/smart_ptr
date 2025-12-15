#include "smart_ptr.h"

utils::sp::sp_counted_base::sp_counted_base()
{
	m_use_count = 1;
	m_weak_count = 1;
}

void utils::sp::sp_counted_base::add_ref_copy()
{
	++m_use_count;
}

void utils::sp::sp_counted_base::add_ref_lock()
{
  ++m_use_count;
}

void utils::sp::sp_counted_base::weak_add_ref()
{
	++m_weak_count;
}

void utils::sp::sp_counted_base::weak_release()
{
	if (--m_weak_count == 0)
	{
		destroy();
	}
}

void utils::sp::sp_counted_base::release()
{
	if (--m_use_count == 0) 
	{
		dispose();
		weak_release();
	}
}

long utils::sp::sp_counted_base::use_count() const
{
	return m_use_count.load();
}
