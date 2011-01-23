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

#include "../src/numerical_coefficient.hpp"

#define BOOST_TEST_MODULE numerical_coefficient_test
#include <boost/test/unit_test.hpp>

#include <boost/mpl/for_each.hpp>
#include <boost/mpl/vector.hpp>
#include <type_traits>

#include "../src/integer.hpp"
#include "../src/mf_int.hpp"

using namespace piranha;

typedef boost::mpl::vector<double,mf_int,integer> types;

struct constructor_tester
{
	template <typename T>
	void operator()(const T &)
	{
		typedef numerical_coefficient<T> nc;
		BOOST_CHECK((std::has_trivial_destructor<T>::value && std::has_trivial_destructor<nc>::value) || (!std::has_trivial_destructor<T>::value && !std::has_trivial_destructor<nc>::value));
		BOOST_CHECK((std::has_trivial_copy_constructor<T>::value && std::has_trivial_copy_constructor<nc>::value) || (!std::has_trivial_copy_constructor<T>::value && !std::has_trivial_copy_constructor<nc>::value));
		BOOST_CHECK_EQUAL(nc().get_value(),T());
		// Try construction from int.
		BOOST_CHECK_EQUAL(nc(3).get_value(),T(3));
		// Assignment from int.
		nc n;
		n = 10;
		BOOST_CHECK_EQUAL(n.get_value(),T(10));
	}
};

BOOST_AUTO_TEST_CASE(numerical_coefficient_constructor_test)
{
	boost::mpl::for_each<types>(constructor_tester());
}