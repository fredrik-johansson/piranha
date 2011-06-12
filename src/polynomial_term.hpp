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

#ifndef PIRANHA_POLYNOMIAL_TERM_HPP
#define PIRANHA_POLYNOMIAL_TERM_HPP

#include <algorithm>
#include <boost/concept/assert.hpp>
#include <type_traits>

#include "base_term.hpp"
#include "concepts/multipliable_coefficient.hpp"
#include "config.hpp"
#include "echelon_descriptor.hpp"
#include "monomial.hpp"
#include "type_traits.hpp"

namespace piranha
{

/// Polynomial term.
/**
 * This class extends piranha::base_term for use in polynomials. The coefficient type \p Cf is generic,
 * the key is piranha::monomial of \p ExpoType.
 * This class is a model of the piranha::concept::MultipliableTerm concept.
 * 
 * \section type_requirements Type requirements
 * 
 * - \p Cf must be a model of piranha::concept::MultipliableCoefficient.
 * - \p ExpoType must be suitable for use in piranha::monomial.
 * 
 * \section exception_safety Exception safety guarantee
 * 
 * This class provides the same guarantee as piranha::base_term.
 * 
 * \section move_semantics Move semantics
 * 
 * Move semantics is equivalent to piranha::base_term's move semantics.
 * 
 * @author Francesco Biscani (bluescarni@gmail.com)
 */
template <typename Cf, typename ExpoType>
class polynomial_term: public base_term<Cf,monomial<ExpoType>,polynomial_term<Cf,ExpoType>>
{
		BOOST_CONCEPT_ASSERT((concept::MultipliableCoefficient<Cf>));
		typedef base_term<Cf,monomial<ExpoType>,polynomial_term<Cf,ExpoType>> base;
	public:
		/// Result type for the multiplication by another term.
		typedef polynomial_term multiplication_result_type;
		/// Defaulted default constructor.
		polynomial_term() = default;
		/// Defaulted copy constructor.
		polynomial_term(const polynomial_term &) = default;
		/// Defaulted move constructor.
		polynomial_term(polynomial_term &&) = default;
		/// Generic constructor.
		/**
		 * Will perfectly forward all arguments to a matching constructor in piranha::base_term.
		 * This constructor is enabled only if the number of variadic arguments is nonzero or
		 * if \p T is not of the same type as \p this.
		 * 
		 * @param[in] arg1 first argument to be forwarded to the base constructor.
		 * @param[in] params other arguments to be forwarded to the base constructor.
		 * 
		 * @throws unspecified any exception thrown by the invoked constructor in piranha::base_term.
		 */
		template <typename T, typename... Args, typename std::enable_if<sizeof...(Args) || !std::is_same<polynomial_term,typename strip_cv_ref<T>::type>::value>::type*& = enabler>
		explicit polynomial_term(T &&arg1, Args && ... params):base(std::forward<T>(arg1),std::forward<Args>(params)...) {}
		/// Defaulted destructor.
		~polynomial_term() = default;
		/// Defaulted copy assignment operator.
		polynomial_term &operator=(const polynomial_term &) = default;
		/// Move assignment operator.
		/**
		 * @param[in] other assignment argument.
		 * 
		 * @return reference to \p this.
		 */
		polynomial_term &operator=(polynomial_term &&other) piranha_noexcept_spec(true)
		{
			base::operator=(std::move(other));
			return *this;
		}
		/// Term multiplication.
		/**
		 * Multiplication of \p this by \p other will produce a single term whose coefficient is the
		 * result of the multiplication of the current coefficient by the coefficient of \p other (via the coefficient's <tt>multiply()</tt> method)
		 * and whose key is the element-by-element sum of the vectors of the exponents of the two terms.
		 * 
		 * @param[out] retval return value of the multiplication.
		 * @param[in] other argument of the multiplication.
		 * @param[in] ed reference echelon descriptor.
		 * 
		 * @throws unspecified any exception thrown by:
		 * - the assignment operators of coefficient and key,
		 * - the <tt>multiply()</tt> method of \p Cf,
		 * - the in-place addition operator of \p ExpoType.
		 * 
		 * \todo use of multiply(): specify that depending on whether one of the coefficients is a series or not,
		 * multiply_by() might be used?
		 * \todo which is the better strategy? In-place or not? Move or not (this is a question for series_multiplier)?
		 */
		template <typename Cf2, typename ExpoType2, typename Term>
		void multiply(polynomial_term &retval, const polynomial_term<Cf2,ExpoType2> &other, const echelon_descriptor<Term> &ed) const
		{
			piranha_assert(other.m_key.size() == this->m_key.size());
			piranha_assert(other.m_key.size() == ed.template get_args<polynomial_term>().size());
			// TODO: fix this.
			//retval.m_cf = this->m_cf.multiply(other.m_cf,ed);
			retval.m_cf = this->m_cf;
			retval.m_cf.multiply_by(other.m_cf,ed);
			// Key part.
			const auto size = this->m_key.size();
			retval.m_key.resize(size);
			std::copy(this->m_key.begin(),this->m_key.end(),retval.m_key.begin());
			for (decltype(this->m_key.size()) i = 0u; i < size; ++i) {
				retval.m_key[i] += other.m_key[i];
			}
		}
};

}

#endif
