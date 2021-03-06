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

#ifndef PIRANHA_DETAIL_DEGREE_COMMONS_HPP
#define PIRANHA_DETAIL_DEGREE_COMMONS_HPP

#include <algorithm>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>

#include "../config.hpp"
#include "../exceptions.hpp"
#include "../symbol_set.hpp"

// Common routines for use in monomial classes for computation of degree and order.

namespace piranha { namespace detail {

// Total degree.
template <typename Retval, typename Container, typename Op>
inline Retval monomial_degree(const Container &c, const Op &op, const symbol_set &args)
{
	if (unlikely(args.size() != c.size())) {
		piranha_throw(std::invalid_argument,"invalid arguments set");
	}
	Retval retval(0);
	for (decltype(c.size()) i = 0u; i < c.size(); ++i) {
		op(retval,c[i]);
	}
	return retval;
}

// Partial degree.
template <typename Retval, typename Container, typename Op>
inline Retval monomial_partial_degree(const Container &c, const Op &op, const std::set<std::string> &active_args, const symbol_set &args)
{
	if (unlikely(args.size() != c.size())) {
		piranha_throw(std::invalid_argument,"invalid arguments set");
	}
	Retval retval(0);
	// Just return zero if the size of the container is also zero.
	if (!c.size()) {
		return retval;
	}
	auto it1 = args.begin();
	auto it2 = active_args.begin();
	// Move forward until element in active args does not preceed the first
	// element in the reference arguments set.
	while (it2 != active_args.end() && *it2 < it1->get_name()) {
		++it2;
	}
	for (decltype(c.size()) i = 0u; i < c.size(); ++i, ++it1) {
		if (it2 == active_args.end()) {
			break;
		}
		if (it1->get_name() == *it2) {
			op(retval,c[i]);
			++it2;
		}
	}
	return retval;
}

// Less-than comparator (we do not use std::less because that can be specialised, and here
// we want to force the use of the builtin operator in conjunction with the is_less_than_comparable
// type trait).
struct less_than_comparator
{
	template <typename T>
	bool operator()(const T &a, const T &b) const
	{
		return a < b;
	}
};

// Generic functor to calculate degree-like properties in series.
template <int N, typename Container, typename Getter>
inline auto generic_series_degree(const Container &c, Getter &g) -> decltype(g(std::declval<typename Container::key_type>()))
{
	typedef typename Container::key_type term_type;
	typedef decltype(g(std::declval<term_type>())) return_type;
	less_than_comparator cmp;
	decltype(c.begin()) it;
	if (N == 0) {
		it = std::max_element(c.begin(),c.end(),[&g,&cmp](const term_type &t1, const term_type &t2) {
			return cmp(g(t1),g(t2));
		});
	} else {
		it = std::min_element(c.begin(),c.end(),[&g,&cmp](const term_type &t1, const term_type &t2) {
			return cmp(g(t1),g(t2));
		});
	}
	return (it == c.end()) ? return_type(0) : g(*it);
}

}}

#endif
