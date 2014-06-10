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

#ifndef PIRANHA_POLYNOMIAL_HPP
#define PIRANHA_POLYNOMIAL_HPP

#include <algorithm>
#include <boost/integer_traits.hpp>
#include <boost/numeric/conversion/cast.hpp>
#include <cmath> // For std::ceil.
#include <condition_variable>
#include <functional> // For std::bind.
#include <initializer_list>
#include <iterator>
#include <map>
#include <mutex>
#include <numeric>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "config.hpp"
#include "debug_access.hpp"
#include "detail/poisson_series_fwd.hpp"
#include "detail/polynomial_fwd.hpp"
#include "echelon_size.hpp"
#include "exceptions.hpp"
#include "forwarding.hpp"
#include "integer.hpp"
#include "kronecker_array.hpp"
#include "kronecker_monomial.hpp"
#include "math.hpp"
#include "polynomial_term.hpp"
#include "power_series.hpp"
#include "series.hpp"
#include "series_multiplier.hpp"
#include "symbol.hpp"
#include "symbol_set.hpp"
#include "t_substitutable_series.hpp"
#include "thread_pool.hpp"
#include "trigonometric_series.hpp"
#include "type_traits.hpp"

#include <boost/timer/timer.hpp>

namespace piranha
{

namespace detail
{

struct polynomial_tag {};

}

/// Polynomial class.
/**
 * This class represents multivariate polynomials as collections of multivariate polynomial terms
 * (represented by the piranha::polynomial_term class). The coefficient
 * type \p Cf represents the ring over which the polynomial is defined, while \p Expo and \p S are used
 * to represent the exponent type and representation in the same way as explained in the documentation of
 * piranha::polynomial_term.
 *
 * Examples:
 @code
 polynomial<double,int>
 @endcode
 * is a multivariate polynomial with double-precision coefficients and \p int exponents.
 @code
 polynomial<double,short,std::integral_constant<std::size_t,5>>
 @endcode
 * is a multivariate polynomial with double-precision coefficients and \p short exponents, up to 5 of which
 * will be stored in static storage.
 @code
 polynomial<double,univariate_monomial<int>>
 @endcode
 * is a univariate polynomial with double-precision coefficients and \p int exponent.
 @code
 polynomial<double,kronecker_monomial<>>
 @endcode
 * is a multivariate polynomial with double-precision coefficients and integral exponents packed into a piranha::kronecker_monomial.
 * 
 * This class satisfies the piranha::is_series type trait.
 * 
 * \section type_requirements Type requirements
 * 
 * \p Cf, \p Expo and \p S must be suitable for use in piranha::polynomial_term.
 * 
 * \section exception_safety Exception safety guarantee
 * 
 * This class provides the same guarantee as piranha::power_series.
 * 
 * \section move_semantics Move semantics
 * 
 * Move semantics is equivalent to piranha::power_series' move semantics.
 * 
 * @author Francesco Biscani (bluescarni@gmail.com)
 * 
 * \todo here, in poisson_series and math::ipow_subs, let ipow_subs accept also C++ integers.
 * This is useful to simplify the notation, and needs not to be done for lower level methods in keys.
 */
template <typename Cf, typename Expo, typename S = std::integral_constant<std::size_t,0u>>
class polynomial:
	public power_series<trigonometric_series<t_substitutable_series<series<polynomial_term<Cf,Expo,S>,
	polynomial<Cf,Expo,S>>,polynomial<Cf,Expo,S>>>>,detail::polynomial_tag
{
		// Make friend with debug class.
		template <typename T>
		friend class debug_access;
		// Make friend with Poisson series.
		template <typename T>
		friend class poisson_series;
		using base = power_series<trigonometric_series<t_substitutable_series<series<polynomial_term<Cf,Expo,S>,
			polynomial<Cf,Expo,S>>,polynomial<Cf,Expo,S>>>>;
		template <typename Str>
		void construct_from_string(Str &&str)
		{
			typedef typename base::term_type term_type;
			// Insert the symbol.
			this->m_symbol_set.add(symbol(std::forward<Str>(str)));
			// Construct and insert the term.
			this->insert(term_type(Cf(1),typename term_type::key_type{1}));
		}
		std::map<std::string,integer> integral_combination() const
		{
			try {
				std::map<std::string,integer> retval;
				for (auto it = this->m_container.begin(); it != this->m_container.end(); ++it) {
					const std::string lin_arg = it->m_key.linear_argument(this->m_symbol_set);
					piranha_assert(retval.find(lin_arg) == retval.end());
					retval[lin_arg] = math::integral_cast(it->m_cf);
				}
				return retval;
			} catch (const std::invalid_argument &) {
				piranha_throw(std::invalid_argument,"polynomial is not an integral linear combination");
			}
		}
		// Subs typedefs.
		// TODO: fix declval usage.
		template <typename T>
		struct subs_type
		{
			typedef typename base::term_type::cf_type cf_type;
			typedef typename base::term_type::key_type key_type;
			typedef decltype(
				(math::subs(std::declval<cf_type>(),std::declval<std::string>(),std::declval<T>()) * std::declval<polynomial>()) *
				std::declval<key_type>().subs(std::declval<symbol>(),std::declval<T>(),std::declval<symbol_set>()).first
			) type;
		};
		template <typename T>
		struct ipow_subs_type
		{
			typedef typename base::term_type::cf_type cf_type;
			typedef typename base::term_type::key_type key_type;
			typedef decltype(
				(math::ipow_subs(std::declval<cf_type>(),std::declval<std::string>(),std::declval<integer>(),std::declval<T>()) * std::declval<polynomial>()) *
				std::declval<key_type>().ipow_subs(std::declval<symbol>(),std::declval<integer>(),std::declval<T>(),std::declval<symbol_set>()).first
			) type;
		};
		// Integration with integrable coefficient.
		polynomial integrate_impl(const symbol &s, const typename base::term_type &term,
			const std::true_type &) const
		{
			typedef typename base::term_type term_type;
			typedef typename term_type::cf_type cf_type;
			typedef typename term_type::key_type key_type;
			// Get the partial degree of the monomial in integral form.
			integer degree;
			try {
				degree = math::integral_cast(term.m_key.degree({s.get_name()},this->m_symbol_set));
			} catch (const std::invalid_argument &) {
				piranha_throw(std::invalid_argument,
					"unable to perform polynomial integration: cannot extract the integral form of an exponent");
			}
			// If the degree is negative, integration by parts won't terminate.
			if (degree.sign() < 0) {
				piranha_throw(std::invalid_argument,
					"unable to perform polynomial integration: negative integral exponent");
			}
			// Initialise retval (this is also the final retval in case the degree of s is zero).
			polynomial retval;
			retval.m_symbol_set = this->m_symbol_set;
			key_type tmp_key = term.m_key;
			cf_type i_cf(math::integrate(term.m_cf,s.get_name()));
			retval.insert(term_type(i_cf,tmp_key));
			for (integer i(1); i <= degree; ++i) {
				// Update coefficient and key.
				i_cf = cf_type(math::integrate(i_cf,s.get_name()));
				auto partial_key = tmp_key.partial(s,this->m_symbol_set);
				i_cf *= std::move(partial_key.first);
				// Account for (-1)**i.
				math::negate(i_cf);
				tmp_key = std::move(partial_key.second);
				retval.insert(term_type(i_cf,tmp_key));
			}
			return retval;
		}
		// Integration with non-integrable coefficient.
		polynomial integrate_impl(const symbol &, const typename base::term_type &,
			const std::false_type &) const
		{
			piranha_throw(std::invalid_argument,"unable to perform polynomial integration: coefficient type is not integrable");
		}
		// Template alias for use in pow() overload. Will check via SFINAE that the base pow() method can be called with argument T
		// and that exponentiation of key type is legal.
		template <typename T, typename Series>
		using pow_ret_type = decltype(std::declval<typename Series::term_type::key_type const &>().pow(std::declval<const T &>(),std::declval<const symbol_set &>()),void(),
			std::declval<series<polynomial_term<Cf,Expo,S>,polynomial<Cf,Expo,S>> const &>().pow(std::declval<const T &>()));
	public:
		/// Defaulted default constructor.
		/**
		 * Will construct a polynomial with zero terms.
		 */
		polynomial() = default;
		/// Defaulted copy constructor.
		polynomial(const polynomial &) = default;
		/// Defaulted move constructor.
		polynomial(polynomial &&) = default;
		/// Constructor from symbol name.
		/**
		 * Will construct a univariate polynomial made of a single term with unitary coefficient and exponent, representing
		 * the symbolic variable \p name. The type of \p name must be a string type (either C or C++).
		 * 
		 * @param[in] name name of the symbolic variable that the polynomial will represent.
		 * 
		 * @throws unspecified any exception thrown by:
		 * - piranha::symbol_set::add(),
		 * - the constructor of piranha::symbol from string,
		 * - the invoked constructor of the coefficient type,
		 * - the invoked constructor of the key type,
		 * - the constructor of the term type from coefficient and key,
		 * - piranha::series::insert().
		 */
		template <typename Str>
		explicit polynomial(Str &&name, typename std::enable_if<
			std::is_same<typename std::decay<Str>::type,std::string>::value ||
			std::is_same<typename std::decay<Str>::type,char *>::value ||
			std::is_same<typename std::decay<Str>::type,const char *>::value>::type * = nullptr) : base()
		{
			construct_from_string(std::forward<Str>(name));
		}
		PIRANHA_FORWARDING_CTOR(polynomial,base)
		/// Trivial destructor.
		~polynomial() noexcept(true)
		{
			PIRANHA_TT_CHECK(is_cf,polynomial);
			PIRANHA_TT_CHECK(is_series,polynomial);
		}
		/// Defaulted copy assignment operator.
		polynomial &operator=(const polynomial &) = default;
		/// Defaulted move assignment operator.
		polynomial &operator=(polynomial &&) = default;
		/// Assignment from symbol name (C string).
		/**
		 * Equivalent to invoking the constructor from symbol name and assigning the result to \p this.
		 * 
		 * @param[in] name name of the symbolic variable that the polynomial will represent.
		 * 
		 * @return reference to \p this.
		 * 
		 * @throws unspecified any exception thrown by the constructor from symbol name.
		 */
		polynomial &operator=(const char *name)
		{
			operator=(polynomial(name));
			return *this;
		}
		/// Assignment from symbol name (C++ string).
		/**
		 * Equivalent to invoking the constructor from symbol name and assigning the result to \p this.
		 * 
		 * @param[in] name name of the symbolic variable that the polynomial will represent.
		 * 
		 * @return reference to \p this.
		 * 
		 * @throws unspecified any exception thrown by the constructor from symbol name.
		 */
		polynomial &operator=(const std::string &name)
		{
			operator=(polynomial(name));
			return *this;
		}
		PIRANHA_FORWARDING_ASSIGNMENT(polynomial,base)
		/// Override default exponentiation method.
		/**
		 * \note
		 * This method is enabled only if piranha::series::pow() can be called with exponent \p x
		 * and the key type can be raised to the power of \p x via its exponentiation method.
		 * 
		 * This exponentiation override will check if the polynomial consists of a single-term with non-unitary
		 * key. In that case, the return polynomial will consist of a single term with coefficient computed via
		 * piranha::math::pow() and key computed via the monomial exponentiation method.
		 * 
		 * Otherwise, the base (i.e., default) exponentiation method will be used.
		 * 
		 * @param[in] x exponent.
		 * 
		 * @return \p this to the power of \p x.
		 * 
		 * @throws unspecified any exception thrown by:
		 * - the <tt>is_unitary()</tt> and exponentiation methods of the key type,
		 * - piranha::math::pow(),
		 * - construction of coefficient, key and term,
		 * - the copy assignment operator of piranha::symbol_set,
		 * - piranha::series::insert() and piranha::series::pow().
		 */
		template <typename T, typename Series = polynomial>
		pow_ret_type<T,Series> pow(const T &x) const
		{
			typedef typename base::term_type term_type;
			typedef typename term_type::cf_type cf_type;
			typedef typename term_type::key_type key_type;
			if (this->size() == 1u && !this->m_container.begin()->m_key.is_unitary(this->m_symbol_set)) {
				cf_type cf(math::pow(this->m_container.begin()->m_cf,x));
				key_type key(this->m_container.begin()->m_key.pow(x,this->m_symbol_set));
				polynomial retval;
				retval.m_symbol_set = this->m_symbol_set;
				retval.insert(term_type(std::move(cf),std::move(key)));
				return retval;
			}
			return static_cast<series<polynomial_term<Cf,Expo,S>,polynomial<Cf,Expo,S>> const *>(this)->pow(x);
		}
		/// Substitution.
		/**
		 * Substitute the symbolic quantity \p name with the generic value \p x. The result for each term is computed
		 * via piranha::math::subs() for the coefficients and via the substitution method for the monomials, and
		 * then assembled into the final return value via multiplications and additions.
		 * 
		 * @param[in] name name of the symbolic variable that will be subject to substitution.
		 * @param[in] x quantity that will be substituted for \p name.
		 * 
		 * @return result of the substitution.
		 * 
		 * @throws unspecified any exception thrown by:
		 * - symbol construction,
		 * - piranha::symbol_set::remove() and assignment operator,
		 * - piranha::math::subs(),
		 * - the substitution method of the monomial type,
		 * - piranha::series::insert(),
		 * - construction, addition and multiplication of the types involved in the computation.
		 * 
		 * \todo type requirements.
		 */
		template <typename T>
		typename subs_type<T>::type subs(const std::string &name, const T &x) const
		{
			typedef typename subs_type<T>::type return_type;
			typedef typename base::term_type term_type;
			typedef typename term_type::cf_type cf_type;
			typedef typename term_type::key_type key_type;
			// Turn name into symbol.
			const symbol s(name);
			// Init return value.
			return_type retval = return_type();
			// Remove the symbol from the current symbol set, if present.
			symbol_set sset(this->m_symbol_set);
			if (std::binary_search(sset.begin(),sset.end(),s)) {
				sset.remove(s);
			}
			const auto it_f = this->m_container.end();
			for (auto it = this->m_container.begin(); it != it_f; ++it) {
				auto cf_sub = math::subs(it->m_cf,name,x);
				auto key_sub = it->m_key.subs(s,x,this->m_symbol_set);
				polynomial tmp_series;
				tmp_series.m_symbol_set = sset;
				tmp_series.insert(term_type(cf_type(1),key_type(key_sub.second)));
				retval += (cf_sub * tmp_series) * key_sub.first;
			}
			return retval;
		}
		/// Substitution of integral power.
		/**
		 * This method will substitute occurrences of \p name to the power of \p n with \p x.
		 * The result for each term is computed via piranha::math::ipow_subs() for the coefficients and via the
		 * corresponding substitution method for the monomials, and then assembled into the final return value via multiplications and additions.
		 * 
		 * @param[in] name name of the symbolic variable that will be subject to substitution.
		 * @param[in] n power of \p name that will be substituted.
		 * @param[in] x quantity that will be substituted for \p name to the power of \p n.
		 * 
		 * @return result of the substitution.
		 * 
		 * @throws unspecified any exception thrown by:
		 * - symbol construction,
		 * - the assignment operator of piranha::symbol_set,
		 * - piranha::math::ipow_subs(),
		 * - the substitution method of the monomial type,
		 * - piranha::series::insert(),
		 * - construction, addition and multiplication of the types involved in the computation.
		 * 
		 * \todo type requirements.
		 */
		template <typename T>
		typename ipow_subs_type<T>::type ipow_subs(const std::string &name, const integer &n, const T &x) const
		{
			typedef typename ipow_subs_type<T>::type return_type;
			typedef typename base::term_type term_type;
			typedef typename term_type::cf_type cf_type;
			typedef typename term_type::key_type key_type;
			// Turn name into symbol.
			const symbol s(name);
			// Init return value.
			return_type retval = return_type();
			const auto it_f = this->m_container.end();
			for (auto it = this->m_container.begin(); it != it_f; ++it) {
				auto cf_sub = math::ipow_subs(it->m_cf,name,n,x);
				auto key_sub = it->m_key.ipow_subs(s,n,x,this->m_symbol_set);
				polynomial tmp_series;
				tmp_series.m_symbol_set = this->m_symbol_set;
				tmp_series.insert(term_type(cf_type(1),key_type(key_sub.second)));
				retval += (cf_sub * tmp_series) * key_sub.first;
			}
			return retval;
		}
		/// Integration.
		/**
		 * This method will attempt to compute the antiderivative of the polynomial term by term. If the term's coefficient does not depend on
		 * the integration variable, the result will be calculated via the integration of the corresponding monomial.
		 * Integration with respect to a variable appearing to the power of -1 will fail.
		 * 
		 * Otherwise, a strategy of integration by parts is attempted, its success depending on the integrability
		 * of the coefficient and on the value of the exponent of the integration variable. The integration will
		 * fail if the exponent is negative or non-integral.
		 * 
		 * This method requires the coefficient type to satisfy the piranha::is_differentiable type trait.
		 * 
		 * @param[in] name integration variable.
		 * 
		 * @return the antiderivative of \p this with respect to \p name.
		 * 
		 * @throws std::invalid_argument if the integration procedure fails.
		 * @throws unspecified any exception thrown by:
		 * - piranha::symbol construction,
		 * - piranha::math::partial(), piranha::math::is_zero(), piranha::math::integrate(), piranha::math::integral_cast() and
		 *   piranha::math::negate(),
		 * - piranha::symbol_set::add() and assignment operator,
		 * - term construction,
		 * - coefficient construction, assignment and arithmetics,
		 * - integration, construction, assignment, differentiation and degree querying methods of the key type,
		 * - insert(),
		 * - series arithmetics.
		 * 
		 * \todo requirements on dividability by degree type, integral_cast, etc.
		 */
		polynomial integrate(const std::string &name) const
		{
			typedef typename base::term_type term_type;
			typedef typename term_type::cf_type cf_type;
			PIRANHA_TT_CHECK(is_differentiable,cf_type);
			// Turn name into symbol.
			const symbol s(name);
			polynomial retval;
			const auto it_f = this->m_container.end();
			for (auto it = this->m_container.begin(); it != it_f; ++it) {
				// If the derivative of the coefficient is null, we just need to deal with
				// the integration of the key.
				if (math::is_zero(math::partial(it->m_cf,name))) {
					polynomial tmp;
					symbol_set sset = this->m_symbol_set;
					// If the variable does not exist in the arguments set, add it.
					if (!std::binary_search(sset.begin(),sset.end(),s)) {
						sset.add(s);
					}
					tmp.m_symbol_set = sset;
					auto key_int = it->m_key.integrate(s,this->m_symbol_set);
					tmp.insert(term_type(it->m_cf,std::move(key_int.second)));
					tmp /= key_int.first;
					retval += std::move(tmp);
				} else {
					retval += integrate_impl(s,*it,std::integral_constant<bool,is_integrable<cf_type>::value>());
				}
			}
			return retval;
		}
};

namespace math
{

/// Specialisation of the piranha::math::subs() functor for polynomial types.
/**
 * This specialisation is activated when \p Series is an instance of piranha::polynomial.
 */
template <typename Series>
struct subs_impl<Series,typename std::enable_if<std::is_base_of<detail::polynomial_tag,Series>::value>::type>
{
	private:
		// TODO: fix declval usage.
		template <typename T>
		struct subs_type
		{
			typedef decltype(std::declval<Series>().subs(std::declval<std::string>(),std::declval<T>())) type;
		};
	public:
		/// Call operator.
		/**
		 * The implementation will use piranha::polynomial::subs().
		 * 
		 * @param[in] s input polynomial.
		 * @param[in] name name of the symbolic variable that will be substituted.
		 * @param[in] x object that will replace \p name.
		 * 
		 * @return output of piranha::polynomial::subs().
		 * 
		 * @throws unspecified any exception thrown by piranha::polynomial::subs().
		 */
		template <typename T>
		typename subs_type<T>::type operator()(const Series &s, const std::string &name, const T &x) const
		{
			return s.subs(name,x);
		}
};

/// Specialisation of the piranha::math::ipow_subs() functor for polynomial types.
/**
 * This specialisation is activated when \p Series is an instance of piranha::polynomial.
 */
template <typename Series>
struct ipow_subs_impl<Series,typename std::enable_if<std::is_base_of<detail::polynomial_tag,Series>::value>::type>
{
	private:
		// TODO: fix declval usage.
		template <typename T>
		struct ipow_subs_type
		{
			typedef decltype(std::declval<Series>().ipow_subs(std::declval<std::string>(),std::declval<integer>(),std::declval<T>())) type;
		};
	public:
		/// Call operator.
		/**
		 * The implementation will use piranha::polynomial::ipow_subs().
		 * 
		 * @param[in] s input polynomial.
		 * @param[in] name name of the symbolic variable that will be substituted.
		 * @param[in] n power of \p name that will be substituted.
		 * @param[in] x object that will replace \p name.
		 * 
		 * @return output of piranha::polynomial::ipow_subs().
		 * 
		 * @throws unspecified any exception thrown by piranha::polynomial::ipow_subs().
		 */
		template <typename T>
		typename ipow_subs_type<T>::type operator()(const Series &s, const std::string &name, const integer &n, const T &x) const
		{
			return s.ipow_subs(name,n,x);
		}
};

/// Specialisation of the piranha::math::integrate() functor for polynomial types.
/**
 * This specialisation is activated when \p Series is an instance of piranha::polynomial.
 */
template <typename Series>
struct integrate_impl<Series,typename std::enable_if<std::is_base_of<detail::polynomial_tag,Series>::value>::type>
{
	/// Call operator.
	/**
	 * The implementation will use piranha::polynomial::integrate().
	 * 
	 * @param[in] s input polynomial.
	 * @param[in] name integration variable.
	 * 
	 * @return antiderivative of \p s with respect to \p name.
	 * 
	 * @throws unspecified any exception thrown by piranha::polynomial::integrate().
	 */
	Series operator()(const Series &s, const std::string &name) const
	{
		return s.integrate(name);
	}
};

}

namespace detail
{

template <typename Series1, typename Series2>
struct kronecker_enabler
{
	PIRANHA_TT_CHECK(is_series,Series1);
	PIRANHA_TT_CHECK(is_series,Series2);
	template <typename Key1, typename Key2>
	struct are_same_kronecker_monomial
	{
		static const bool value = false;
	};
	template <typename T>
	struct are_same_kronecker_monomial<kronecker_monomial<T>,kronecker_monomial<T>>
	{
		static const bool value = true;
	};
	typedef typename Series1::term_type::key_type key_type1;
	typedef typename Series2::term_type::key_type key_type2;
	static const bool value = std::is_base_of<detail::polynomial_tag,Series1>::value &&
		std::is_base_of<detail::polynomial_tag,Series2>::value && are_same_kronecker_monomial<key_type1,key_type2>::value;
};

}

/// Series multiplier specialisation for polynomials with Kronecker monomials.
/**
 * This specialisation of piranha::series_multiplier is enabled when both \p Series1 and \p Series2 are instances of
 * piranha::polynomial with monomials represented as piranha::kronecker_monomial of the same type.
 * This multiplier will employ optimized algorithms that take advantage of the properties of Kronecker monomials.
 * It will also take advantage of piranha::math::multiply_accumulate() in place of plain coefficient multiplication
 * when possible.
 * 
 * \section exception_safety Exception safety guarantee
 * 
 * This class provides the same guarantee as the non-specialised piranha::series_multiplier.
 * 
 * \section move_semantics Move semantics
 * 
 * Move semantics is equivalent to piranha::series_multiplier's move semantics.
 * 
 * \todo optimize task list in single thread and maybe also for small operands -> make it a vector I guess, instead of a set.
 */
template <typename Series1, typename Series2>
class series_multiplier<Series1,Series2,typename std::enable_if<detail::kronecker_enabler<Series1,Series2>::value>::type>:
	public series_multiplier<Series1,Series2,int>
{
		PIRANHA_TT_CHECK(is_series,Series1);
		PIRANHA_TT_CHECK(is_series,Series2);
		typedef typename Series1::term_type::key_type::value_type value_type;
		typedef kronecker_array<value_type> ka;
	public:
		/// Base multiplier type.
		typedef series_multiplier<Series1,Series2,int> base;
		/// Alias for term type of \p Series1.
		typedef typename Series1::term_type term_type1;
		/// Alias for term type of \p Series2.
		typedef typename Series2::term_type term_type2;
		/// Alias for the return type.
		typedef typename base::return_type return_type;
		/// Constructor.
		/**
		 * Will call the base constructor and additionally check that the result of the multiplication will not overflow
		 * the representation limits of piranha::kronecker_monomial. In such a case, a runtime error will be produced.
		 * 
		 * @param[in] s1 first series operand.
		 * @param[in] s2 second series operand.
		 * 
		 * @throws std::invalid_argument if the the result of the multiplication overflows the representation limits of
		 * piranha::kronecker_monomial.
		 * @throws unspecified any exception thrown by the base constructor.
		 */
		explicit series_multiplier(const Series1 &s1, const Series2 &s2):base(s1,s2)
		{
			if (unlikely(this->m_s1->empty() || this->m_s2->empty())) {
				return;
			}
			// NOTE: here we are sure about this since the symbol set in a series should never
			// overflow the size of the limits, as the check for compatibility in Kronecker monomial
			// would kick in.
			piranha_assert(this->m_s1->m_symbol_set.size() < ka::get_limits().size());
			piranha_assert(this->m_s1->m_symbol_set == this->m_s2->m_symbol_set);
			const auto &limits = ka::get_limits()[this->m_s1->m_symbol_set.size()];
			// NOTE: We need to check that the exponents of the monomials in the result do not
			// go outside the bounds of the Kronecker codification. We need to unpack all monomials
			// in the operands and examine them, we cannot operate on the codes for this.
			auto it1 = this->m_v1.begin();
			auto it2 = this->m_v2.begin();
			// Initialise minmax values.
			std::vector<std::pair<value_type,value_type>> minmax_values1;
			std::vector<std::pair<value_type,value_type>> minmax_values2;
			auto tmp_vec1 = (*it1)->m_key.unpack(this->m_s1->m_symbol_set);
			auto tmp_vec2 = (*it2)->m_key.unpack(this->m_s1->m_symbol_set);
			// Bounds of the Kronecker representation for each component.
			const auto &minmax_vec = std::get<0u>(limits);
			std::transform(tmp_vec1.begin(),tmp_vec1.end(),std::back_inserter(minmax_values1),[](const value_type &v) {
				return std::make_pair(v,v);
			});
			std::transform(tmp_vec2.begin(),tmp_vec2.end(),std::back_inserter(minmax_values2),[](const value_type &v) {
				return std::make_pair(v,v);
			});
			// Find the minmaxs.
			for (; it1 != this->m_v1.end(); ++it1) {
				tmp_vec1 = (*it1)->m_key.unpack(this->m_s1->m_symbol_set);
				piranha_assert(tmp_vec1.size() == minmax_values1.size());
				std::transform(minmax_values1.begin(),minmax_values1.end(),tmp_vec1.begin(),minmax_values1.begin(),
					[](const std::pair<value_type,value_type> &p, const value_type &v) {
						return std::make_pair(
							v < p.first ? v : p.first,
							v > p.second ? v : p.second
						);
				});
			}
			for (; it2 != this->m_v2.end(); ++it2) {
				tmp_vec2 = (*it2)->m_key.unpack(this->m_s2->m_symbol_set);
				piranha_assert(tmp_vec2.size() == minmax_values2.size());
				std::transform(minmax_values2.begin(),minmax_values2.end(),tmp_vec2.begin(),minmax_values2.begin(),
					[](const std::pair<value_type,value_type> &p, const value_type &v) {
						return std::make_pair(
							v < p.first ? v : p.first,
							v > p.second ? v : p.second
						);
				});
			}
			// Compute the sum of the two minmaxs, using multiprecision to avoid overflow.
			// NOTE: first store in m_minmax_values the ranges of the result only, below we will update this with the range
			// of the operands.
			std::transform(minmax_values1.begin(),minmax_values1.end(),minmax_values2.begin(),
				std::back_inserter(m_minmax_values),[](const std::pair<value_type,value_type> &p1,
				const std::pair<value_type,value_type> &p2) {
					return std::make_pair(integer(p1.first) + integer(p2.first),integer(p1.second) + integer(p2.second));
			});
			piranha_assert(m_minmax_values.size() == minmax_vec.size());
			piranha_assert(m_minmax_values.size() == minmax_values1.size());
			piranha_assert(m_minmax_values.size() == minmax_values2.size());
			for (decltype(m_minmax_values.size()) i = 0u; i < m_minmax_values.size(); ++i) {
				if (unlikely(m_minmax_values[i].first < -minmax_vec[i] || m_minmax_values[i].second > minmax_vec[i])) {
					piranha_throw(std::overflow_error,"Kronecker monomial components are out of bounds");
				}
				// Update with the ranges of the operands.
				m_minmax_values[i] = std::minmax({m_minmax_values[i].first,
					integer(minmax_values1[i].first),integer(minmax_values2[i].first),m_minmax_values[i].second,
					integer(minmax_values1[i].second),integer(minmax_values2[i].second)});
			}
		}
		/// Perform multiplication.
		/**
		 * @return the result of the multiplication of the input series operands.
		 * 
		 * @throws unspecified any exception thrown by:
		 * - (unlikely) conversion errors between numeric types,
		 * - the public interface of piranha::hash_set,
		 * - piranha::series_multiplier::estimate_final_series_size(),
		 * - piranha::series_multiplier::blocked_multiplication(),
		 * - piranha::math::multiply_accumulate() on the coefficient types.
		 */
		return_type operator()() const
		{
			return execute();
		}
	private:
		typedef typename std::vector<term_type1 const *>::size_type index_type;
		typedef typename Series1::size_type bucket_size_type;
		// This is a bucket region, i.e., a _closed_ interval [a,b] of bucket indices in a hash set.
		typedef std::pair<bucket_size_type,bucket_size_type> region_type;
		// Block-by-block multiplication task.
		struct task_type
		{
			// First block: semi-open range [a,b[ of indices in first input series.
			std::pair<index_type,index_type>	m_b1;
			// Second block (indices in second input series).
			std::pair<index_type,index_type>	m_b2;
			// First region memory region involved in the multiplication.
			region_type				m_r1;
			// Second memory region.
			region_type				m_r2;
			// Boolean flag to signal if the second region is present or not.
			bool					m_second_region;
		};
		// Create task from indices i in first series, j in second series (semi-open intervals).
		task_type task_from_indices(const index_type &i_start, const index_type &i_end,
			const index_type &j_start, const index_type &j_end, const return_type &retval) const
		{
			const auto &v1 = this->m_v1;
			const auto &v2 = this->m_v2;
			piranha_assert(i_start < i_end && j_start < j_end);
			piranha_assert(i_end <= v1.size() && j_end <= v2.size());
			const auto b_count = retval.m_container.bucket_count();
			piranha_assert(b_count > 0u);
			region_type r1{0u,0u}, r2{0u,0u};
			bool second_region = false;
			// Addition is safe because of the limits on the max bucket count of hash_set.
			const auto a = retval.m_container._bucket_from_hash(v1[i_start]->hash()) +
				retval.m_container._bucket_from_hash(v2[j_start]->hash()),
				// NOTE: we are sure that the tasks are not empty, so the -1 is safe.
				b = retval.m_container._bucket_from_hash(v1[i_end - 1u]->hash()) +
				retval.m_container._bucket_from_hash(v2[j_end - 1u]->hash());
			// NOTE: using <= here as [a,b] is now a closed interval.
			piranha_assert(a <= b);
			piranha_assert(b <= b_count * 2u - 2u);
			if (b < b_count || a >= b_count) {
				r1.first = a % b_count;
				r1.second = b % b_count;
			} else {
				second_region = true;
				r1.first = a;
				r1.second = b_count - 1u;
				r2.first = 0u;
				r2.second = b % b_count;
			}
			// Correct the case in which a second region reaches the first one (thus
			// covering the whole range of buckets).
			if (second_region && r2.second >= r1.first) {
				second_region = false;
				r1.first = 0u;
				r1.second = b_count - 1u;
				r2.first = 0u;
				r2.second = 0u;
			}
			piranha_assert(r1.first <= r1.second);
			piranha_assert(!second_region || (r2.first <= r2.second && r2.second < r1.first));
			return task_type{std::make_pair(i_start,i_end),std::make_pair(j_start,j_end),r1,r2,second_region};
		}
		// Functor to check if region r does not overlap any of the busy ones.
		template <typename RegionSet>
		static bool region_checker(const region_type &r, const RegionSet &busy_regions)
		{
			if (busy_regions.empty()) {
				return true;
			}
			// NOTE: lower bound means that it_b->first >= r.first.
			auto it_b = busy_regions.lower_bound(r);
			// Handle end().
			if (it_b == busy_regions.end()) {
				// NOTE: safe because busy_regions is not empty.
				--it_b;
				// Now check that the region in it_b does not overlap with
				// the current one.
				piranha_assert(it_b->first < r.first);
				return it_b->second < r.first;
			}
			if (r.second >= it_b->first) {
				// The end point of r1 overlaps the start point of *it_b, no good.
				return false;
			}
			// Lastly, we have to check that the previous element (if any) does not
			// overlap r.first.
			if (it_b != busy_regions.begin()) {
				--it_b;
				if (it_b->second >= r.first) {
					return false;
				}
			}
			return true;
		}
		// Check that a set of busy regions is well-formed, for debug purposes.
		template <typename RegionSet>
		static bool region_set_checker(const RegionSet &busy_regions)
		{
			if (busy_regions.empty()) {
				return true;
			}
			const auto it_f = busy_regions.end();
			auto it = busy_regions.begin();
			auto prev_it(it);
			++it;
			for (; it != it_f; ++it, ++prev_it) {
				if (it->first > it->second || prev_it->first > prev_it->second) {
					return false;
				}
				if (prev_it->second >= it->first) {
					return false;
				}
			}
			return true;
		}
		// Have to place this here because if created as a lambda, it will result in a
		// compiler error in GCC 4.5. In GCC 4.6 there is no such problem.
		// This will sort tasks according to the initial writing position in the hash table
		// of the result.
		struct sparse_task_sorter
		{
			explicit sparse_task_sorter(const return_type &retval,const std::vector<term_type1 const *> &v1,
				const std::vector<term_type2 const *> &v2):
				m_retval(retval),m_v1(v1),m_v2(v2)
			{}
			bool operator()(const task_type &t1, const task_type &t2) const
			{
				piranha_assert(m_retval.m_container.bucket_count());
				// NOTE: here we are sure there is no overflow as the max size of the hash table is 2 ** (n - 1), hence the highest
				// possible bucket is 2 ** (n - 1) - 1, so the highest value of the sum will be 2 ** n - 2. But the range of the size type
				// is [0, 2 ** n - 1], so it is safe.
				// NOTE: might use faster mod calculation here, but probably not worth it.
				// NOTE: because tasks cannot contain empty intervals, the start of each interval will be a valid index (i.e., not end())
				// in the term pointers vectors.
				piranha_assert(t1.m_b1.first < m_v1.size() && t2.m_b1.first < m_v1.size() &&
					t1.m_b2.first < m_v2.size() && t2.m_b2.first < m_v2.size() &&
					t1.m_b1.first < t1.m_b1.second && t1.m_b2.first < t1.m_b2.second &&
					t2.m_b1.first < t2.m_b1.second && t2.m_b2.first < t2.m_b2.second
				);
				return (m_retval.m_container._bucket_from_hash(m_v1[t1.m_b1.first]->hash()) +
					m_retval.m_container._bucket_from_hash(m_v2[t1.m_b2.first]->hash())) %
					m_retval.m_container.bucket_count() <
					(m_retval.m_container._bucket_from_hash(m_v1[t2.m_b1.first]->hash()) +
					m_retval.m_container._bucket_from_hash(m_v2[t2.m_b2.first]->hash())) %
					m_retval.m_container.bucket_count();
			}
			const return_type			&m_retval;
			const std::vector<term_type1 const *>	&m_v1;
			const std::vector<term_type2 const *>	&m_v2;
		};
		return_type execute() const
		{
			const index_type size1 = this->m_v1.size(), size2 = boost::numeric_cast<index_type>(this->m_v2.size());
			// Do not do anything if one of the two series is empty, just return an empty series.
			if (unlikely(!size1 || !size2)) {
				return return_type{};
			}
			// This check is done here to avoid controlling the number of elements of the output series
			// at every iteration of the functor.
			const auto max_size = integer(size1) * size2;
			if (unlikely(max_size > boost::integer_traits<bucket_size_type>::const_max)) {
				piranha_throw(std::overflow_error,"possible overflow in series size");
			}
			// First, let's get the estimation on the size of the final series.
			return_type retval;
			retval.m_symbol_set = this->m_s1->m_symbol_set;
			typename Series1::size_type estimate;
			// Use the sparse functor for the estimation.
			estimate = base::estimate_final_series_size(sparse_functor<>(&this->m_v1[0u],size1,&this->m_v2[0u],size2,retval));
			// Correct the unlikely case of zero estimate.
			if (unlikely(!estimate)) {
				estimate = 1u;
			}
			// Rehash the retun value's container accordingly.
			// NOTE: if something goes wrong here, no big deal as retval is still empty.
			retval.m_container.rehash(boost::numeric_cast<typename Series1::size_type>(std::ceil(static_cast<double>(estimate) /
				retval.m_container.max_load_factor())));
			piranha_assert(retval.m_container.bucket_count());
			if ((integer(size1) * integer(size2)) / estimate > 200) {
				dense_multiplication(retval);
			} else {
				sparse_multiplication<sparse_functor<>>(retval);
			}
			// Trace the result of estimation.
			this->trace_estimates(retval.size(),estimate);
			return retval;
		}
		// Utility function to determine block sizes.
		static std::pair<integer,integer> get_block_sizes(const index_type &size1, const index_type &size2)
		{
			const integer block_size(512u), job_size = block_size.pow(2u);
			// Rescale the block sizes according to the relative sizes of the input series.
			auto block_size1 = (block_size * size1) / size2,
				block_size2 = (block_size * size2) / size1;
			// Avoid having zero block sizes, or block sizes exceeding job_size.
			if (!block_size1) {
				block_size1 = 1u;
			} else if (block_size1 > job_size) {
				block_size1 = job_size;
			}
			if (!block_size2) {
				block_size2 = 1u;
			} else if (block_size2 > job_size) {
				block_size2 = job_size;
			}
			return std::make_pair(std::move(block_size1),std::move(block_size2));
		}
		// Compare regions by lower bound.
		struct region_comparer
		{
			bool operator()(const region_type &r1, const region_type &r2) const
			{
				return r1.first < r2.first;
			}
		};
		// Function to remove the regions associated to a task from the set of busy regions.
		template <typename RegionSet>
		static void cleanup_regions(RegionSet &busy_regions, const task_type &task)
		{
			auto tmp_it = busy_regions.find(task.m_r1);
			if (tmp_it != busy_regions.end()) {
				busy_regions.erase(tmp_it);
			}
			// Deal with second region only if present.
			if (!task.m_second_region) {
				return;
			}
			tmp_it = busy_regions.find(task.m_r2);
			if (tmp_it != busy_regions.end()) {
				busy_regions.erase(tmp_it);
			}
			piranha_assert(region_set_checker(busy_regions));
		}
		// Dense task sorter.
		template <typename NKType1, typename NKType2>
		struct dts_type
		{
			explicit dts_type(const std::vector<NKType1> &nk1, const std::vector<NKType2> &nk2):
				m_new_keys1(nk1),m_new_keys2(nk2)
			{}
			template <typename Task>
			bool operator()(const Task &t1, const Task &t2) const noexcept
			{
				return m_new_keys1[t1.m_b1.first].first + m_new_keys2[t1.m_b2.first].first <
					m_new_keys1[t2.m_b1.first].first + m_new_keys2[t2.m_b2.first].first;
			}
			const std::vector<NKType1>	&m_new_keys1;
			const std::vector<NKType2>	&m_new_keys2;
		};
		// Dense multiplication method.
		void dense_multiplication(return_type &retval) const
		{
			// Vectors of minimum / maximum values, cast to hardware int.
			std::vector<value_type> mins;
			std::transform(m_minmax_values.begin(),m_minmax_values.end(),std::back_inserter(mins),[](const std::pair<integer,integer> &p) {
				return static_cast<value_type>(p.first);
			});
			std::vector<value_type> maxs;
			std::transform(m_minmax_values.begin(),m_minmax_values.end(),std::back_inserter(maxs),[](const std::pair<integer,integer> &p) {
				return static_cast<value_type>(p.second);
			});
			// Build the encoding vector.
			std::vector<value_type> c_vec;
			integer f_delta(1);
			std::transform(m_minmax_values.begin(),m_minmax_values.end(),std::back_inserter(c_vec),
				[&f_delta](const std::pair<integer,integer> &p) -> value_type {
					auto old(f_delta);
					f_delta *= p.second - p.first + 1;
					return static_cast<value_type>(old);
			});
			// Try casting final delta.
			(void)static_cast<value_type>(f_delta);
			// Compute hmax and hmin.
			piranha_assert(m_minmax_values.size() == c_vec.size());
			const auto h_minmax = std::inner_product(m_minmax_values.begin(),m_minmax_values.end(),c_vec.begin(),
				std::make_pair(integer(0),integer(0)),
				[](const std::pair<integer,integer> &p1, const std::pair<integer,integer> &p2) {
					return std::make_pair(p1.first + p2.first,p1.second + p2.second);
				},[](const std::pair<integer,integer> &p, const value_type &value) {
					return std::make_pair(p.first * value,p.second * value);
				}
			);
			piranha_assert(f_delta == h_minmax.second - h_minmax.first + 1);
			// Try casting hmax and hmin.
			const auto hmin = static_cast<value_type>(h_minmax.first);
			const auto hmax = static_cast<value_type>(h_minmax.second);
			// Encoding functor.
			typedef typename term_type1::key_type::v_type unpack_type;
			auto encoder = [&c_vec,hmin,&mins](const unpack_type &v) -> value_type {
				piranha_assert(c_vec.size() == v.size());
				piranha_assert(c_vec.size() == mins.size());
				decltype(mins.size()) i = 0u;
				value_type retval = std::inner_product(c_vec.begin(),c_vec.end(),v.begin(),value_type(0),
					std::plus<value_type>(),[&i,&mins](const value_type &c, const value_type &n) -> value_type {
						const decltype(mins.size()) old_i = i;
						++i;
						piranha_assert(n >= mins[old_i]);
						return static_cast<value_type>(c * (n - mins[old_i]));
					}
				);
				return static_cast<value_type>(retval + hmin);
			};
			// Build copies of the input keys repacked according to the new Kronecker substitution. Attach
			// also a pointer to the term.
			typedef std::pair<value_type,term_type1 const *> new_key_type1;
			typedef std::pair<value_type,term_type2 const *> new_key_type2;
			std::vector<new_key_type1> new_keys1;
			std::vector<new_key_type2> new_keys2;
			std::transform(this->m_v1.begin(),this->m_v1.end(),std::back_inserter(new_keys1),
				[this,encoder](term_type1 const *ptr) {
				return std::make_pair(encoder(ptr->m_key.unpack(this->m_s1->m_symbol_set)),ptr);
			});
			std::transform(this->m_v2.begin(),this->m_v2.end(),std::back_inserter(new_keys2),
				[this,encoder](term_type2 const *ptr) {
				return std::make_pair(encoder(ptr->m_key.unpack(this->m_s1->m_symbol_set)),ptr);
			});
			// Sort the the new keys.
			std::sort(new_keys1.begin(),new_keys1.end(),[](const new_key_type1 &p1, const new_key_type1 &p2) {
				return p1.first < p2.first;
			});
			std::sort(new_keys2.begin(),new_keys2.end(),[](const new_key_type2 &p1, const new_key_type2 &p2) {
				return p1.first < p2.first;
			});
			// Store the sizes and compute the block sizes.
			const index_type size1 = boost::numeric_cast<index_type>(new_keys1.size()),
				size2 = boost::numeric_cast<index_type>(new_keys2.size());
			piranha_assert(size1 == this->m_s1->size());
			piranha_assert(size2 == this->m_s2->size());
			const auto bsizes = get_block_sizes(size1,size2);
			// Cast to hardware integers.
			const auto bsize1 = static_cast<index_type>(bsizes.first), bsize2 = static_cast<index_type>(bsizes.second);
			// Build the list of tasks.
			dts_type<new_key_type1,new_key_type2> dense_task_sorter(new_keys1,new_keys2);
			std::multiset<task_type,dts_type<new_key_type1,new_key_type2>> task_list(dense_task_sorter);
			decltype(task_list.insert(std::declval<task_type>())) ins_result;
			auto dense_task_from_indices = [hmin,hmax,&new_keys1,&new_keys2](const index_type &i_start, const index_type &i_end,
				const index_type &j_start, const index_type &j_end) -> task_type
			{
				piranha_assert(i_end <= new_keys1.size() && j_end <= new_keys2.size());
				piranha_assert(i_start < i_end && j_start < j_end);
				const bucket_size_type a = boost::numeric_cast<bucket_size_type>((new_keys1[i_start].first + new_keys2[j_start].first) - hmin);
				const bucket_size_type b = boost::numeric_cast<bucket_size_type>((new_keys1[i_end - 1u].first + new_keys2[j_end - 1u].first) - hmin);
				piranha_assert(a <= b);
				piranha_assert(b <= boost::numeric_cast<bucket_size_type>(hmax - hmin));
				return task_type{std::make_pair(i_start,i_end),std::make_pair(j_start,j_end),
					std::make_pair(a,b),std::make_pair(bucket_size_type(0u),bucket_size_type(0u)),false};
			};
			for (index_type i = 0u; i < size1 / bsize1; ++i) {
				for (index_type j = 0u; j < size2 / bsize2; ++j) {
					ins_result = task_list.insert(dense_task_from_indices(i * bsize1,(i + 1u) * bsize1,j * bsize2,(j + 1u) * bsize2));
					piranha_assert(ins_result->m_b1.first != ins_result->m_b1.second && ins_result->m_b2.first != ins_result->m_b2.second);
				}
				if (size2 % bsize2) {
					ins_result = task_list.insert(dense_task_from_indices(i * bsize1,(i + 1u) * bsize1,(size2 / bsize2) * bsize2,size2));
					piranha_assert(ins_result->m_b1.first != ins_result->m_b1.second && ins_result->m_b2.first != ins_result->m_b2.second);
				}
			}
			if (size1 % bsize1) {
				for (index_type j = 0u; j < size2 / bsize2; ++j) {
					ins_result = task_list.insert(dense_task_from_indices((size1 / bsize1) * bsize1,size1,j * bsize2,(j + 1u) * bsize2));
					piranha_assert(ins_result->m_b1.first != ins_result->m_b1.second && ins_result->m_b2.first != ins_result->m_b2.second);
				}
				if (size2 % bsize2) {
					ins_result = task_list.insert(dense_task_from_indices((size1 / bsize1) * bsize1,size1,(size2 / bsize2) * bsize2,size2));
					piranha_assert(ins_result->m_b1.first != ins_result->m_b1.second && ins_result->m_b2.first != ins_result->m_b2.second);
				}
			}
			// Prepare the storage for multiplication.
			std::vector<typename term_type1::cf_type> cf_vector;
			cf_vector.resize(boost::numeric_cast<decltype(cf_vector.size())>((hmax - hmin) + 1));
			// Get the number of threads.
			typedef decltype(this->determine_n_threads()) thread_size_type;
			const thread_size_type n_threads = this->determine_n_threads();
			if (n_threads == 1u) {
				// Single-thread multiplication.
				const auto it_f = task_list.end();
				for (auto it = task_list.begin(); it != it_f; ++it) {
					const index_type i_start = it->m_b1.first, j_start = it->m_b2.first,
						i_end = it->m_b1.second, j_end = it->m_b2.second;
					piranha_assert(i_end > i_start && j_end > j_start);
					for (index_type i = i_start; i < i_end; ++i) {
						for (index_type j = j_start; j < j_end; ++j) {
							const auto idx = (new_keys1[i].first + new_keys2[j].first) - hmin;
							piranha_assert(idx < boost::numeric_cast<value_type>(cf_vector.size()));
							math::multiply_accumulate(cf_vector[static_cast<decltype(cf_vector.size())>(idx)],
								new_keys1[i].second->m_cf,new_keys2[j].second->m_cf);
						}
					}
				}
			} else {
				// Set of busy bucket regions, ordered by starting point.
				// NOTE: we do not need a multiset, as regions that compare equal (i.e., same starting point) will not exist
				// at the same time in the set by design.
				region_comparer rc;
				std::set<region_type,region_comparer> busy_regions(rc);
				// Synchronization.
				std::mutex m;
				std::condition_variable cond;
				// Thread function.
				auto thread_function = [&cond,&m,&task_list,&busy_regions,&new_keys1,&new_keys2,hmin,&cf_vector,this] () {
					task_type task;
					while (true) {
						{
							// First, lock down everything.
							std::unique_lock<std::mutex> lock(m);
							if (task_list.empty()) {
								break;
							}
							// Look for a suitable task.
							const auto it_f = task_list.end();
							auto it = task_list.begin();
							for (; it != it_f; ++it) {
								piranha_assert(!it->m_second_region);
								// Check the region.
								if (this->region_checker(it->m_r1,busy_regions)) {
									try {
										// The region is ok, insert it and break the cycle.
										auto tmp = busy_regions.insert(it->m_r1);
										(void)tmp;
										piranha_assert(tmp.second);
										piranha_assert(this->region_set_checker(busy_regions));
									} catch (...) {
										// NOTE: the idea here is that in case of errors
										// we want to restore the original situation
										// as if nothing happened.
										this->cleanup_regions(busy_regions,*it);
										throw;
									}
									break;
								}
							}
							// We might have identified a suitable task, check it is not end().
							if (it == it_f) {
								// The thread can't do anything, will have to wait until something happens
								// and then re-identify a good task.
								cond.wait(lock);
								continue;
							}
							// Now we have a good task, pop it from the task list.
							task = *it;
							task_list.erase(it);
						}
						try {
							// Perform the multiplication on the selected task.
							const index_type i_start = task.m_b1.first, j_start = task.m_b2.first,
								i_end = task.m_b1.second, j_end = task.m_b2.second;
							piranha_assert(i_end > i_start && j_end > j_start);
							for (index_type i = i_start; i < i_end; ++i) {
								for (index_type j = j_start; j < j_end; ++j) {
									const auto idx = (new_keys1[i].first + new_keys2[j].first) - hmin;
									piranha_assert(idx < boost::numeric_cast<value_type>(cf_vector.size()));
									math::multiply_accumulate(cf_vector[static_cast<decltype(cf_vector.size())>(idx)],
										new_keys1[i].second->m_cf,new_keys2[j].second->m_cf);
								}
							}
						} catch (...) {
							// Re-acquire the lock.
							std::lock_guard<std::mutex> lock(m);
							// Cleanup the regions.
							this->cleanup_regions(busy_regions,task);
							// Notify all waiting threads that a region was removed from the busy set.
							cond.notify_all();
							throw;
						}
						{
							// Re-acquire the lock.
							std::lock_guard<std::mutex> lock(m);
							// Take out the regions in which we just wrote from the set of busy regions.
							this->cleanup_regions(busy_regions,task);
							// Notify all waiting threads that a region was removed from the busy set.
							cond.notify_all();
						}
					}
				};
				// NOTE: one of the fundamental requirements here is that we wait for pending
				// tasks, in case of errors, before getting out of this function: thread_function
				// contains references to local variables, if we get out of here before the tasks are finished
				// memory corruption will occur.
				future_list<decltype(thread_pool::enqueue(0u,thread_function))> f_list;
				try {
					for (thread_size_type i = 0u; i < n_threads; ++i) {
						// NOTE: enqueue() will either happen or it won't, we only care
						// about memory allocation errors in push_back() here. In such case,
						// push_back() will wait on the temporary future from enqueue
						// before returning the exception.
						f_list.push_back(thread_pool::enqueue(i,thread_function));
					}
					// First let's wait for everything to finish.
					f_list.wait_all();
					// Then, let's handle the exceptions.
					f_list.get_all();
				} catch (...) {
					// Make sure any pending task is finished -> this is for
					// the case the exception was thrown in the future creation
					// loop. It is safe to call this again in case the exception
					// being handled is generated by get_all(), as wait_all() will check
					// the validity of the future before calling wait().
					f_list.wait_all();
					throw;
				}
			}
			// Build the return value.
			// Append the final delta to the coding vector for use in the decoding routine.
			c_vec.push_back(static_cast<value_type>(f_delta));
			// Temp vector for decoding.
			std::vector<value_type> tmp_v;
			tmp_v.resize(boost::numeric_cast<decltype(tmp_v.size())>(this->m_s1->m_symbol_set.size()));
			piranha_assert(c_vec.size() - 1u == tmp_v.size());
			piranha_assert(mins.size() == tmp_v.size());
			auto decoder = [&tmp_v,&c_vec,&mins,&maxs](const value_type &n) {
				decltype(c_vec.size()) i = 0u;
				std::generate(tmp_v.begin(),tmp_v.end(),
					[&n,&i,&mins,&maxs,&c_vec]() -> value_type
				{
					auto retval = (n % c_vec[i + 1u]) / c_vec[i] + mins[i];
					piranha_assert(retval >= mins[i] && retval <= maxs[i]);
					++i;
					return static_cast<value_type>(retval);
				});
			};
			const auto cf_size = cf_vector.size();
			term_type1 tmp_term;
			for (decltype(cf_vector.size()) i = 0u; i < cf_size; ++i) {
				if (!math::is_zero(cf_vector[i])) {
					tmp_term.m_cf = std::move(cf_vector[i]);
					decoder(boost::numeric_cast<value_type>(i));
					tmp_term.m_key = decltype(tmp_term.m_key)(tmp_v.begin(),tmp_v.end());
					retval.insert(std::move(tmp_term));
				}
			}
		}
		template <typename Functor>
		void sparse_multiplication_new(return_type &retval) const
		{
			// Build vectors of bucket positions into retval.
			using bucket_idx_type = decltype(retval.m_container._bucket_from_hash(0u));
			std::vector<std::pair<term_type1 const *,bucket_idx_type>> bvi1;
			std::vector<std::pair<term_type2 const *,bucket_idx_type>> bvi2;
			std::transform(this->m_v1.begin(),this->m_v1.end(),std::back_inserter(bvi1),[&retval](term_type1 const *ptr) {
				return std::make_pair(ptr,retval.m_container._bucket_from_hash(ptr->hash()));
			});
			std::transform(this->m_v2.begin(),this->m_v2.end(),std::back_inserter(bvi2),[&retval](term_type2 const *ptr) {
				return std::make_pair(ptr,retval.m_container._bucket_from_hash(ptr->hash()));
			});
			auto cmp1 = [](const std::pair<term_type1 const *,bucket_idx_type> &p1,
				const std::pair<term_type1 const *,bucket_idx_type> &p2)
			{
				return p1.second < p2.second;
			};
			std::sort(bvi1.begin(),bvi1.end(),cmp1);
			auto cmp2 = [](const std::pair<term_type2 const *,bucket_idx_type> &p1,
				const std::pair<term_type2 const *,bucket_idx_type> &p2)
			{
				return p1.second < p2.second;
			};
			std::sort(bvi2.begin(),bvi2.end(),cmp2);
			const bucket_idx_type bucket_count = retval.m_container.bucket_count();
			typedef decltype(this->determine_n_threads()) thread_size_type;
			const thread_size_type n_threads = this->determine_n_threads();
			// Buckets per thread.
			const auto bpt = bucket_count / n_threads;
			bucket_size_type insertion_count(0);
std::mutex m;
std::vector<integer> n_mults(n_threads);
			auto thread_function = [n_threads,bpt,bucket_count,&bvi1,&bvi2,cmp2,&retval,&m,&n_mults,&insertion_count](thread_size_type idx) {
				using it_type2 = decltype(bvi2.cbegin());
				// This is the range of bucket indices into which the thread is allowed
				// to write.
				const auto a = static_cast<bucket_size_type>(bpt * idx),
					b = (idx == n_threads - 1u) ? bucket_count : static_cast<bucket_size_type>(bpt * (idx + 1u));
{
std::lock_guard<std::mutex> lock(m);
std::cout << "idx,a,b = " << idx << ',' << a << ',' << b << '\n';
}
				// Vector of term multiplication tasks to be undertaken by the thread.
				std::vector<std::tuple<term_type1 const *,it_type2,it_type2>> tasks;
				it_type2 start, end;
				const unsigned block_size = 512;
				for (const auto &bv1: bvi1) {
					const auto n = bv1.second;
					start = (a < n) ? bvi2.cbegin() :
						std::lower_bound(bvi2.cbegin(),bvi2.cend(),
						std::make_pair(bvi2.cbegin()->first,bucket_idx_type(a - n)),cmp2);
					end = (b < n) ? bvi2.cbegin() :
						std::lower_bound(bvi2.cbegin(),bvi2.cend(),
						std::make_pair(bvi2.cbegin()->first,bucket_idx_type(b - n)),cmp2);
					if (start != end) {
						while (end - start > block_size) {
							tasks.push_back(std::make_tuple(bv1.first,start,start + block_size));
							start = start + block_size;
						}
						tasks.push_back(std::make_tuple(bv1.first,start,end));
					}
					start = ((a + bucket_count) < n) ? bvi2.cbegin() :
						std::lower_bound(bvi2.cbegin(),bvi2.cend(),
						std::make_pair(bvi2.cbegin()->first,bucket_idx_type((a + bucket_count) - n)),cmp2);
					end = ((b + bucket_count) < n) ? bvi2.cbegin() :
						std::lower_bound(bvi2.cbegin(),bvi2.cend(),
						std::make_pair(bvi2.cbegin()->first,bucket_idx_type((b + bucket_count) - n)),cmp2);
					if (start != end) {
						while (end - start > block_size) {
							tasks.push_back(std::make_tuple(bv1.first,start,start + block_size));
							start = start + block_size;
						}
						tasks.push_back(std::make_tuple(bv1.first,start,end));
					}
				}
				std::sort(tasks.begin(),tasks.end(),[bucket_count,&retval](const std::tuple<term_type1 const *,it_type2,it_type2> &t1,const std::tuple<term_type1 const *,it_type2,it_type2> &t2) {
					return (retval.m_container._bucket_from_hash(std::get<0u>(t1)->hash()) + retval.m_container._bucket_from_hash(std::get<1u>(t1)->first->hash())) % bucket_count
						<
						(retval.m_container._bucket_from_hash(std::get<0u>(t2)->hash()) + retval.m_container._bucket_from_hash(std::get<1u>(t2)->first->hash())) % bucket_count;
				});
{
std::lock_guard<std::mutex> lock(m);
//std::cout << "idx,task size: " << idx << ',' << tasks.size() << '\n';
}
//				for (const auto &t: tasks) {
//					const auto ptr_b = retval.m_container._bucket_from_hash(std::get<0u>(t)->hash());
//					const auto low = std::get<1u>(t)->second, high = (std::get<2u>(t) - 1)->second;
//					if ((ptr_b + low) % bucket_count < a || (ptr_b + high) % bucket_count >= b) {
//						std::abort();
//					}
//					n_mults[idx] += std::distance(std::get<1u>(t),std::get<2u>(t));
//				}
				std::vector<term_type2 const *> t2_copy;
				std::transform(bvi2.cbegin(),bvi2.cend(),std::back_inserter(t2_copy),[](const std::pair<term_type2 const *,bucket_idx_type> &p) {return p.first;});
				// Perform the multiplication on the selected task.
				using fast_functor_type = typename Functor::fast_rebind;
				bucket_size_type ins_count(0);
				for (const auto &t: tasks) {
					auto t1_ptr = std::get<0u>(t);
					auto start = std::get<1u>(t), end = std::get<2u>(t);
					auto offset = start - bvi2.begin();
					auto size = end - start;
					fast_functor_type f(&t1_ptr,1u,&t2_copy[0] + offset,size,retval);
					for (decltype(size) i = 0; i < size; ++i) {
						f(0u,i);
						f.insert();
					}
					ins_count += f.m_insertion_count;
				}
				std::lock_guard<std::mutex> lock(m);
				insertion_count += ins_count;
				
				
				
//				const index_type i_start = task.m_b1.first, j_start = task.m_b2.first,
//					i_end = task.m_b1.second, j_end = task.m_b2.second;
//				piranha_assert(i_end > i_start && j_end > j_start);
//				const index_type i_size = i_end - i_start, j_size = j_end - j_start;
//				fast_functor_type f(&this->m_v1[0u] + i_start,i_size,&this->m_v2[0u] + j_start,j_size,retval);
//				for (index_type i = 0u; i < i_size; ++i) {
//					for (index_type j = 0u; j < j_size; ++j) {
//						f(i,j);
//						f.insert();
//					}
//				}
//				tmp_ins_count = f.m_insertion_count;
			};
			future_list<decltype(thread_pool::enqueue(0u,thread_function,0u))> f_list;
			try {
				for (thread_size_type i = 0u; i < n_threads; ++i) {
					f_list.push_back(thread_pool::enqueue(i,thread_function,i));
				}
				// First let's wait for everything to finish.
				f_list.wait_all();
				// Then, let's handle the exceptions.
				f_list.get_all();
				// Finally, fix the series.
				sanitize_series(retval,insertion_count,n_threads);
			} catch (...) {
				f_list.wait_all();
				// Clean up and re-throw.
				retval.m_container.clear();
				throw;
			}
//std::cout << "tot nmults: " << std::accumulate(n_mults.begin(),n_mults.end(),integer(0)) << '\n';
			
			
			
			
			
			
			
//std::cout << "bucket count: " << bucket_count << '\n';
//std::cout << "min1: " << std::min_element(bvi1.begin(),bvi1.end(),cmp1)->second << '\n';
//std::cout << "max1: " << std::max_element(bvi1.begin(),bvi1.end(),cmp1)->second << '\n';
//std::cout << "min2: " << std::min_element(bvi2.begin(),bvi2.end(),cmp2)->second << '\n';
//std::cout << "max2: " << std::max_element(bvi2.begin(),bvi2.end(),cmp2)->second << '\n';
//const auto a = (bucket_count / n_threads) * 1u, b = (bucket_count / n_threads) * 2u;
//std::cout << "thread 0: " << a << ',' << b << '\n';

//std::cout << (std::upper_bound(bvi2.begin(),bvi2.end(),std::make_pair(bvi2.begin()->first,b - bvi1.begin()->second),cmp2))->second << '\n';
//std::cout << (--std::upper_bound(bvi2.begin(),bvi2.end(),std::make_pair(bvi2.begin()->first,b - bvi1.begin()->second),cmp2))->second << '\n';

		}
		template <typename Functor>
		void sparse_multiplication(return_type &retval) const
		{
			const index_type size1 = this->m_v1.size(), size2 = boost::numeric_cast<index_type>(this->m_v2.size());
if (size1 > 5000u && size2 > 5000u) {
std::cout << "fooooo\n";
boost::timer::auto_cpu_timer t;
	sparse_multiplication_new<Functor>(retval);
	return;
}
			// Sort the input terms according to the position of the Kronecker keys in the estimated return value.
			auto sorter1 = [&retval](term_type1 const *ptr1, term_type1 const *ptr2) {
				return retval.m_container._bucket_from_hash(ptr1->hash()) < retval.m_container._bucket_from_hash(ptr2->hash());
			};
			auto sorter2 = [&retval](term_type2 const *ptr1, term_type2 const *ptr2) {
				return retval.m_container._bucket_from_hash(ptr1->hash()) < retval.m_container._bucket_from_hash(ptr2->hash());
			};
			std::sort(this->m_v1.begin(),this->m_v1.end(),sorter1);
			std::sort(this->m_v2.begin(),this->m_v2.end(),sorter2);
			// Start defining the blocks for series multiplication.
			const auto bsizes = get_block_sizes(size1,size2);
			// Cast to hardware integers.
			const auto bsize1 = static_cast<index_type>(bsizes.first), bsize2 = static_cast<index_type>(bsizes.second);
			// Create the list of tasks.
			// NOTE: the way tasks are created, there is never an empty task - all intervals have nonzero sizes.
			// The task are sorted according to the index of the first bucket of retval that will be written to,
			// so we need a multiset as different tasks might have the same starting position.
			std::multiset<task_type,sparse_task_sorter> task_list(sparse_task_sorter(retval,this->m_v1,this->m_v2));
			decltype(task_list.insert(std::declval<task_type>())) ins_result;
			for (index_type i = 0u; i < size1 / bsize1; ++i) {
				for (index_type j = 0u; j < size2 / bsize2; ++j) {
					ins_result = task_list.insert(task_from_indices(i * bsize1,(i + 1u) * bsize1,j * bsize2,(j + 1u) * bsize2,retval));
					piranha_assert(ins_result->m_b1.first != ins_result->m_b1.second && ins_result->m_b2.first != ins_result->m_b2.second);
				}
				if (size2 % bsize2) {
					ins_result = task_list.insert(task_from_indices(i * bsize1,(i + 1u) * bsize1,(size2 / bsize2) * bsize2,size2,retval));
					piranha_assert(ins_result->m_b1.first != ins_result->m_b1.second && ins_result->m_b2.first != ins_result->m_b2.second);
				}
			}
			if (size1 % bsize1) {
				for (index_type j = 0u; j < size2 / bsize2; ++j) {
					ins_result = task_list.insert(task_from_indices((size1 / bsize1) * bsize1,size1,j * bsize2,(j + 1u) * bsize2,retval));
					piranha_assert(ins_result->m_b1.first != ins_result->m_b1.second && ins_result->m_b2.first != ins_result->m_b2.second);
				}
				if (size2 % bsize2) {
					ins_result = task_list.insert(task_from_indices((size1 / bsize1) * bsize1,size1,(size2 / bsize2) * bsize2,size2,retval));
					piranha_assert(ins_result->m_b1.first != ins_result->m_b1.second && ins_result->m_b2.first != ins_result->m_b2.second);
				}
			}
			typedef decltype(this->determine_n_threads()) thread_size_type;
			const thread_size_type n_threads = this->determine_n_threads();
			if (n_threads == 1u) {
				// Perform the multiplication. We need this try/catch because, by using the fast interface,
				// in case of an error the container in retval could be left in an inconsistent state.
				try {
					sparse_single_thread<Functor>(retval,task_list);
				} catch (...) {
					retval.m_container.clear();
					throw;
				}
			} else {
				// Insertion counter.
				bucket_size_type insertion_count = 0u;
				// Set of busy bucket regions, ordered by starting point.
				// NOTE: we do not need a multiset, as regions that compare equal (i.e., same starting point) will not exist
				// at the same time in the set by design.
				region_comparer rc;
				std::set<region_type,region_comparer> busy_regions(rc);
				// Synchronization.
				std::mutex m;
				std::condition_variable cond;
				// Thread function.
				auto thread_function = [&cond,&m,&insertion_count,&task_list,&busy_regions,&retval,this] () {
					task_type task;
					while (true) {
						{
							// First, lock down everything.
							std::unique_lock<std::mutex> lock(m);
							if (task_list.empty()) {
								break;
							}
							// Look for a suitable task.
							const auto it_f = task_list.end();
							auto it = task_list.begin();
							for (; it != it_f; ++it) {
								// Check the regions.
								if (this->region_checker(it->m_r1,busy_regions) &&
									(!it->m_second_region || this->region_checker(it->m_r2,busy_regions)))
								{
									try {
										// The regions are ok, insert them and break the cycle.
										auto tmp = busy_regions.insert(it->m_r1);
										piranha_assert(tmp.second);
										if (it->m_second_region) {
											tmp = busy_regions.insert(it->m_r2);
											piranha_assert(tmp.second);
										}
										piranha_assert(this->region_set_checker(busy_regions));
									} catch (...) {
										// NOTE: the idea here is that in case of errors
										// we want to restore the original situation
										// as if nothing happened.
										this->cleanup_regions(busy_regions,*it);
										throw;
									}
									break;
								}
							}
							// We might have identified a suitable task, check it is not end().
							if (it == it_f) {
								// The thread can't do anything, will have to wait until something happens
								// and then re-identify a good task.
								cond.wait(lock);
								continue;
							}
							// Now we have a good task, pop it from the task list.
							task = *it;
							task_list.erase(it);
						}
						bucket_size_type tmp_ins_count = 0u;
						try {
							// Perform the multiplication on the selected task.
							typedef typename Functor::fast_rebind fast_functor_type;
							const index_type i_start = task.m_b1.first, j_start = task.m_b2.first,
								i_end = task.m_b1.second, j_end = task.m_b2.second;
							piranha_assert(i_end > i_start && j_end > j_start);
							const index_type i_size = i_end - i_start, j_size = j_end - j_start;
							fast_functor_type f(&this->m_v1[0u] + i_start,i_size,&this->m_v2[0u] + j_start,j_size,retval);
							for (index_type i = 0u; i < i_size; ++i) {
								for (index_type j = 0u; j < j_size; ++j) {
									f(i,j);
									f.insert();
								}
							}
							tmp_ins_count = f.m_insertion_count;
						} catch (...) {
							// Re-acquire the lock.
							std::lock_guard<std::mutex> lock(m);
							// Cleanup the regions.
							this->cleanup_regions(busy_regions,task);
							// Notify all waiting threads that a region was removed from the busy set.
							cond.notify_all();
							throw;
						}
						{
							// Re-acquire the lock.
							std::lock_guard<std::mutex> lock(m);
							// Update the insertion count.
							insertion_count += tmp_ins_count;
							// Take out the regions in which we just wrote from the set of busy regions.
							this->cleanup_regions(busy_regions,task);
							// Notify all waiting threads that a region was removed from the busy set.
							cond.notify_all();
						}
					}
				};
				future_list<decltype(thread_pool::enqueue(0u,thread_function))> f_list;
				try {
					for (thread_size_type i = 0u; i < n_threads; ++i) {
						f_list.push_back(thread_pool::enqueue(i,thread_function));
					}
					// First let's wait for everything to finish.
					f_list.wait_all();
					// Then, let's handle the exceptions.
					f_list.get_all();
					// Finally, fix the series.
					sanitize_series(retval,insertion_count,n_threads);
				} catch (...) {
					f_list.wait_all();
					// Clean up and re-throw.
					retval.m_container.clear();
					throw;
				}
			}
		}
		// Single-thread sparse multiplication.
		template <typename Functor, typename TaskList>
		void sparse_single_thread(return_type &retval, const TaskList &task_list) const
		{
			typedef typename Functor::fast_rebind fast_functor_type;
			bucket_size_type insertion_count = 0u;
			const auto it_f = task_list.end();
			for (auto it = task_list.begin(); it != it_f; ++it) {
				const index_type i_start = it->m_b1.first, j_start = it->m_b2.first,
					i_end = it->m_b1.second, j_end = it->m_b2.second;
				piranha_assert(i_end > i_start && j_end > j_start);
				const index_type i_size = i_end - i_start, j_size = j_end - j_start;
				fast_functor_type f(&this->m_v1[0u] + i_start,i_size,&this->m_v2[0u] + j_start,j_size,retval);
				for (index_type i = 0u; i < i_size; ++i) {
					for (index_type j = 0u; j < j_size; ++j) {
						f(i,j);
						f.insert();
					}
				}
				insertion_count += f.m_insertion_count;
			}
			sanitize_series(retval,insertion_count);
		}
		// Sanitize series after completion of sparse multiplication.
		static void sanitize_series(return_type &retval, const bucket_size_type &insertion_count, unsigned n_threads = 1u)
		{
			// Here we have to do the following things:
			// - check ignorability of terms,
			// - cope with excessive load factor,
			// - update the size of the series.
			// Compatibility is not a concern for polynomials.
			// First, let's fix the size of inserted terms.
			retval.m_container._update_size(insertion_count);
			// Second, erase the ignorable terms.
			if (n_threads == 1u) {
				const auto it_f = retval.m_container.end();
				for (auto it = retval.m_container.begin(); it != it_f;) {
					if (unlikely(it->is_ignorable(retval.m_symbol_set))) {
						it = retval.m_container.erase(it);
					} else {
						++it;
					}
				}
			} else {
				const auto b_count = retval.m_container.bucket_count();
				piranha_assert(b_count);
				// Adjust the number of threads if they are more than the bucket count.
				const unsigned nt = (n_threads <= b_count) ? n_threads : static_cast<unsigned>(b_count);
				std::mutex m;
				auto eraser = [b_count,&retval,&m](const bucket_size_type &start, const bucket_size_type &end) {
					piranha_assert(start < end && end <= b_count);
					bucket_size_type erase_count = 0u;
					std::vector<term_type1> term_list;
					for (bucket_size_type i = start; i != end; ++i) {
						term_list.clear();
						const auto &bl = retval.m_container._get_bucket_list(i);
						const auto it_f = bl.end();
						for (auto it = bl.begin(); it != it_f; ++it) {
							if (unlikely(it->is_ignorable(retval.m_symbol_set))) {
								term_list.push_back(*it);
							}
						}
						for (auto it = term_list.begin(); it != term_list.end(); ++it) {
							// NOTE: must use _erase to avoid concurrent modifications
							// to the number of elements in the table.
							retval.m_container._erase(retval.m_container._find(*it,i));
							++erase_count;
						}
					}
					if (erase_count) {
						std::lock_guard<std::mutex> lock(m);
						piranha_assert(erase_count <= retval.m_container.size());
						retval.m_container._update_size(retval.m_container.size() - erase_count);
					}
				};
				future_list<decltype(thread_pool::enqueue(0u,eraser,bucket_size_type(),bucket_size_type()))> f_list;
				try {
					for (unsigned i = 0u; i < nt; ++i) {
						const auto start = (b_count / nt) * i, end = (i == nt - 1u) ? b_count : (b_count / nt) * (i + 1u);
						f_list.push_back(thread_pool::enqueue(i,eraser,start,end));
					}
					// First let's wait for everything to finish.
					f_list.wait_all();
					// Then, let's handle the exceptions.
					f_list.get_all();
				} catch (...) {
					f_list.wait_all();
					// Clean up and re-throw.
					retval.m_container.clear();
					throw;
				}
			}
			// Finally, cope with excessive load factor.
			if (unlikely(retval.m_container.load_factor() > retval.m_container.max_load_factor())) {
				retval.m_container.rehash(
					boost::numeric_cast<bucket_size_type>(std::ceil(static_cast<double>(retval.m_container.size()) / retval.m_container.max_load_factor()))
				);
			}
		}
		// Functor for use in sparse multiplication.
		template <bool FastMode = false>
		struct sparse_functor: base::default_functor
		{
			// Fast version of functor.
			typedef sparse_functor<true> fast_rebind;
			// NOTE: here the coefficient of m_tmp gets default-inited explicitly by the default constructor of base term.
			explicit sparse_functor(term_type1 const **ptr1, const index_type &s1,
				term_type2 const **ptr2, const index_type &s2, return_type &retval):
				base::default_functor(ptr1,s1,ptr2,s2,retval),
				m_cached_i(0u),m_cached_j(0u),m_insertion_count(0u)
			{}
			void operator()(const index_type &i, const index_type &j) const
			{
				using int_type = decltype(this->m_ptr1[i]->m_key.get_int());
				piranha_assert(i < this->m_s1 && j < this->m_s2);
				this->m_tmp.m_key.set_int(static_cast<int_type>(this->m_ptr1[i]->m_key.get_int() + this->m_ptr2[j]->m_key.get_int()));
				m_cached_i = i;
				m_cached_j = j;
			}
			void insert() const
			{
				// NOTE: be very careful: every kind of optimization in here must involve only the key part,
				// as the coefficient part is still generic.
				auto &container = this->m_retval.m_container;
				auto &tmp = this->m_tmp;
				const auto &cf1 = this->m_ptr1[m_cached_i]->m_cf;
				const auto &cf2 = this->m_ptr2[m_cached_j]->m_cf;
				const auto &args = this->m_retval.m_symbol_set;
				// Prepare the return series.
				piranha_assert(!FastMode || container.bucket_count());
				if (!FastMode && unlikely(!container.bucket_count())) {
					container._increase_size();
				}
				// Try to locate the term into retval.
				auto bucket_idx = container._bucket(tmp);
				const auto it = container._find(tmp,bucket_idx);
				if (it == container.end()) {
					// NOTE: the check here is done outside.
					piranha_assert(container.size() < boost::integer_traits<bucket_size_type>::const_max);
					// Term is new. Handle the case in which we need to rehash because of load factor.
					if (!FastMode && unlikely(static_cast<double>(container.size() + bucket_size_type(1u)) / static_cast<double>(container.bucket_count()) >
						container.max_load_factor()))
					{
						container._increase_size();
						// We need a new bucket index in case of a rehash.
						bucket_idx = container._bucket(tmp);
					}
					// TODO optimize this in case of series and integer (?), now it is optimized for simple coefficients.
					// Note that the best course of action here for integer multiplication would seem to resize tmp.m_cf appropriately
					// and then use something like mpz_mul. On the other hand, it seems like in the insertion below we need to perform
					// a copy anyway, so insertion with move seems ok after all? Mmmh...
					// TODO: other important thing: for coefficient series, we probably want to insert with move() below,
					// as we are not going to re-use the allocated resources in tmp.m_cf -> in other words, optimize this
					// as much as possible.
					// Take care of multiplying the coefficient.
					tmp.m_cf = cf1;
					tmp.m_cf *= cf2;
					// Insert and update size.
					// NOTE: in fast mode, the check will be done at the end.
					// NOTE: the counters are protected from overflows by the check done in the operator() of the multiplier.
					if (FastMode) {
						container._unique_insert(tmp,bucket_idx);
						++m_insertion_count;
					} else if (likely(!tmp.is_ignorable(args))) {
						container._unique_insert(tmp,bucket_idx);
						container._update_size(container.size() + 1u);
					}
				} else {
					// Assert the existing term is not ignorable, in non-fast mode.
					piranha_assert(FastMode || !it->is_ignorable(args));
					if (FastMode) {
						// In fast mode we do not care if this throws or produces a null coefficient,
						// as we will be dealing with that from outside.
						math::multiply_accumulate(it->m_cf,cf1,cf2);
					} else {
						// Cleanup function.
						auto cleanup = [&it,&args,&container]() {
							if (unlikely(it->is_ignorable(args))) {
								container.erase(it);
							}
						};
						try {
							math::multiply_accumulate(it->m_cf,cf1,cf2);
							// Check if the term has become ignorable or incompatible after the modification.
							cleanup();
						} catch (...) {
							// In case of exceptions, do the check before re-throwing.
							cleanup();
							throw;
						}
					}
				}
			}
			mutable index_type		m_cached_i;
			mutable index_type		m_cached_j;
			mutable bucket_size_type	m_insertion_count;
		};
	private:
		// Vector of closed ranges of the exponents in both the operands and the result.
		std::vector<std::pair<integer,integer>> m_minmax_values;
};

}

#endif
