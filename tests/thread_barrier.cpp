/***************************************************************************
 *   Copyright (C) 2009-2011 by Francesco Biscani                          *
 *   bluescarni@gmail.com                                                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include "../src/thread_barrier.hpp"

#define BOOST_TEST_MODULE thread_barrier_test
#include <boost/test/unit_test.hpp>

#include <functional>
#include <future>

#include "../src/environment.hpp"
#include "../src/thread_pool.hpp"

BOOST_AUTO_TEST_CASE(thread_barrier_test_01)
{
	piranha::environment env;
	const unsigned n_threads = 100;
	piranha::thread_barrier tb(n_threads);
	piranha::thread_pool::resize(n_threads);
	piranha::future_list<std::future<void>> f_list;
	for (unsigned i = 0; i < n_threads; ++i) {
		BOOST_CHECK_NO_THROW(f_list.push_back(piranha::thread_pool::enqueue(i,std::bind([&tb](unsigned x, unsigned y) {tb.wait();(void)(x + y);},i,i + 1))));
	}
	BOOST_CHECK_NO_THROW(f_list.wait_all());
	BOOST_CHECK_NO_THROW(f_list.wait_all());
}
