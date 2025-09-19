#include "../include/fraction.h"
#include <cmath>
#include <numeric>
#include <sstream>
#include <regex>

big_int gcd(big_int a, big_int b) {
    if (a < 0) {
        a = 0_bi - a;
    }

    if (b < 0) {
        b = 0_bi - b;
    }

    while (b != 0) {
        big_int tmp = b;
        b = a % b;
        a = tmp;
    }

    return a;
}

void fraction::optimise()
{
    if (_denominator == 0) {
        throw std::invalid_argument("Denominator cannot be zero");
    }

    if (_numerator == 0) {
        _denominator = 1;
        return;
    }

    big_int divisor = gcd(_numerator, _denominator);
    _numerator /= divisor;
    _denominator /= divisor;

    if (_denominator < 0) {
        _numerator *= -1;
        _denominator *= -1;
    }
}

fraction::fraction(pp_allocator<big_int::value_type> allocator) : _numerator(0, allocator), _denominator(1, allocator) {}

fraction &fraction::operator+=(fraction const &other) &
{
    _numerator = _numerator * other._denominator + _denominator * other._numerator;
    _denominator = _denominator * other._denominator;
    optimise();
    return *this;
}

fraction fraction::operator+(fraction const &other) const
{
    fraction tmp = *this;
    tmp += other;
    return tmp;
}

fraction &fraction::operator-=(fraction const &other) &
{
    _numerator = _numerator * other._denominator - _denominator * other._numerator;
    _denominator = _denominator * other._denominator;
    optimise();
    return *this;
}

fraction fraction::operator-(fraction const &other) const
{
    fraction tmp = *this;
    tmp -= other;
    return tmp;
}

fraction fraction::operator-() const
{
    fraction result = *this;
    result._numerator *= -1;
    result.optimise();
    return result;
}

fraction &fraction::operator*=(fraction const &other) &
{
    _numerator *= other._numerator;
    _denominator *= other._denominator;
    optimise();
    return *this;
}

fraction fraction::operator*(fraction const &other) const
{
    fraction tmp = *this;
    tmp *= other;
    return tmp;
}

fraction &fraction::operator/=(fraction const &other) &
{
    if (other._denominator == 0) {
        throw std::invalid_argument("Denominator cannot be zero");
    }

    _numerator *= other._denominator;
    _denominator *= other._numerator;
    optimise();
    return *this;
}

fraction fraction::operator/(fraction const &other) const
{
    fraction tmp = *this;
    tmp /= other;
    return tmp;
}

bool fraction::operator==(fraction const &other) const noexcept
{
    if (_numerator == other._numerator && _denominator == other._denominator) {
        return true;
    }

    return false;
}

std::partial_ordering fraction::operator<=>(const fraction& other) const noexcept
{
    big_int const _this = _numerator * other._denominator;
    big_int const _other = _denominator * other._numerator;

    if (_this < _other) {
        return std::partial_ordering::less;
    } else if(_this > _other) {
        return std::partial_ordering::greater;
    }

    return std::partial_ordering::equivalent;
}

std::ostream &operator<<(std::ostream &stream, fraction const &obj)
{
    stream << obj.to_string();
    return stream;
}

std::istream &operator>>(std::istream &stream, fraction &obj)
{
    std::string input;
    stream >> input;

    std::regex fraction_regex(R"(^([-+]?[0-9]+)(?:/([-+]?[0-9]+))?$)");
    std::smatch match;

    if (std::regex_match(input, match, fraction_regex)) {
        big_int numerator(match[1].str(), 10);
        big_int denominator(1);

        if (match[2].matched) {
            denominator = big_int(match[2].str(), 10);
        }

        obj = fraction(numerator, denominator);
    } else {
        throw std::invalid_argument("Invalid fraction format");
    }

    return stream;
}

std::string fraction::to_string() const
{
    std::stringstream ss;

    if (_denominator < 0) {
        fraction tmp = *this;
        tmp._numerator *= -1;
        tmp._denominator *= -1;
        ss << tmp.to_string();
        return ss.str();
    }

    ss << _numerator << "/" << _denominator;
    return ss.str();
}

fraction fraction::sin(fraction const &epsilon) const
{
    // x - x^3/3! + x^5/5! - x^7/7! + ...

    fraction x = *this;
    fraction result = x;
    fraction term = x;
    big_int n = 1;

    while (term > epsilon || term < -epsilon) {
        n += 2;
        term *= -x * x / (fraction(big_int((n - 1) * (n)), big_int(1)));
        result += term;
    }

    return result;
}

fraction fraction::arcsin(fraction const &epsilon) const {
    // arcsin(x) = x + (1/2)x^3/3 + (1*3)/(2*4)x^5/5 + (1*3*5)/(2*4*6)x^7/7

    if (*this > fraction(1,1) || *this < fraction(1,-1)) {
        throw std::runtime_error("|x| must be <= 1 for arcsin");
    }

    fraction x = *this;
    fraction result = x;
    fraction term = x;
    big_int n = 1;

    while (term > epsilon || term < -epsilon) {
        term *= x * x * fraction(big_int(n * 2 - 1), big_int(n * 2));
        result += term / fraction(big_int(n * 2 + 1), 1);
        ++n;
    }

    return result;
}

fraction fraction::cos(fraction const &epsilon) const
{
    // 1 - x^2/2! + x^4/4! - ...

    fraction x = *this;
    fraction result(1, 1);
    big_int n = 1;
    fraction term = result;

    while (term > epsilon || term < -epsilon) {
        n += 2;
        term *= -(x * x) / (fraction(big_int((n - 1) * (n - 2)), big_int(1)));
        result += term;
    }

    return result;
}

fraction fraction::arccos(fraction const &epsilon) const {

    if (*this < fraction(-1, 1) || *this > fraction(1, 1)) {
        throw std::domain_error("Arccos is undefined for |x| > 1");
    }

    return pi_2 - this->arcsin(epsilon);
}

fraction fraction::tg(fraction const &epsilon) const
{
    fraction sin_x = this->sin(epsilon);
    fraction cos_x = this->cos(epsilon);

    if (cos_x._denominator == 0) {
        throw std::runtime_error("Division by zero (cos(x) = 0)");
    }

    return sin_x / cos_x;
}

fraction fraction::arctg(fraction const &epsilon) const {
    // arctg(x) = x - x^3/3 + x^5/5 - x^7/7 + ...

    if (*this > fraction(1,1) || *this < fraction(1,-1)) {
        throw std::domain_error("Use arctg identity for |x| > 1");
    }

    fraction x = *this;
    fraction result = x;
    fraction term = x;
    big_int n = 1;

    while (term > epsilon || term < -epsilon) {
        n += 2;
        term *= -(x * x);
        result += term / fraction(big_int(n), 1);
    }

    return result;
}

fraction fraction::ctg(fraction const &epsilon) const
{
    fraction sin_x = this->sin(epsilon);
    fraction cos_x = this->cos(epsilon);

    if (sin_x._denominator == 0) {
        throw std::runtime_error("Division by zero (sin(x) = 0)");
    }

    return cos_x / sin_x;
}

fraction fraction::arcctg(fraction const &epsilon) const {
    // arccot(x) = π/2 - arctan(x)

    return pi_2 - this->arctg(epsilon);
}

fraction fraction::sec(fraction const &epsilon) const
{
    // sec = 1 / cos

    fraction cos_x = this->cos(epsilon);

    if (cos_x._denominator == 0) {
        throw std::runtime_error("Division by zero (cos(x) = 0)");
    }

    return fraction(1, 1) / cos_x;
}

fraction fraction::arcsec(fraction const &epsilon) const {
    // arcsec(x) = arccos(1/x)

    fraction inv_x = fraction(1, 1) / (*this);
    return inv_x.arccos(epsilon);
}

fraction fraction::cosec(fraction const &epsilon) const
{
    // cosec = 1 / sin

    fraction sin_x = this->sin(epsilon);

    if (sin_x._denominator == 0) {
        throw std::runtime_error("Division by zero (sin(x) = 0)");
    }

    return fraction(1,1) / sin_x;
}

fraction fraction::arccosec(fraction const &epsilon) const {
    // arccsc(x) = arcsin(1/x)

    fraction inv_x = fraction(1, 1) / (*this);
    return inv_x.arcsin(epsilon);
}

fraction fraction::pow(size_t degree) const
{
    if (degree == 0) {
        return fraction(1, 1);
    }

    fraction base = *this;
    fraction result(1, 1);

    while (degree > 0) {
        if (degree & 1) {
            result *= base;
            --degree;
        }

        base *= base;
        degree >>= 1;
    }

    return result;
}

fraction fraction::root(size_t n, fraction const &epsilon) const
{
    if (n == 0) {
        throw std::invalid_argument("Degree cannot be 0");
    }

    if (n == 1) {
        return *this;
    }

    if (_denominator < 0 && n % 2 == 0) {
        throw std::invalid_argument("Degree cannot be even");
    }

    fraction A = *this;
    if (A._numerator < 0) {
        A = -A;
    }

    fraction x_k = A / fraction(n, 1);
    fraction x_k_prev;

    do {
        x_k_prev = x_k;
        fraction x_pow = x_k.pow(n - 1);

        if (x_pow._numerator == 0) {
            throw std::runtime_error("Division by zero in root calculation");
        }

        // xk+1 = ((n - 1) * xk + A/(xk^n-1)) / n, A - исходное число, xk - текущее приближение
        x_k = (fraction(n - 1, 1) * x_k + A / x_pow) / fraction(n, 1);
    } while ((x_k - x_k_prev > epsilon) || (x_k_prev - x_k > epsilon));

    if (_numerator < 0 && n % 2 == 1) {
        x_k = -x_k;
    }

    return x_k;
}

fraction fraction::log2(fraction const &epsilon) const
{
    // log2 = ln x / ln 2

    if (_denominator <= 0 || _numerator <= 0) {
        throw std::runtime_error("Logarithm of non-positive number is undefined");
    }

    fraction ln2 = fraction(2, 1).ln(epsilon);
    return this->ln(epsilon) / ln2;
}

fraction fraction::ln(fraction const &epsilon) const
{
    // ln(x) = 2 * [(x-1)/(x+1) + 1/3*(x-1)^3/(x+1)^3 + 1/5*(x-1)^5/(x+1)^5 + ...]

    if (_denominator <= 0 || _numerator <= 0) {
        throw std::runtime_error("Logarithm of non-positive number is undefined");
    }

    fraction x = (*this - fraction(1, 1)) / (*this + fraction(1, 1));

    if (x._numerator == 0) {
        return fraction(0, 1);
    }

    fraction result = x;
    fraction term = x;
    big_int n = 1;

    while (term > epsilon || term < -epsilon) {
        term *= x * x * fraction(big_int(n * 2 - 1), big_int(n * 2 + 1));
        ++n;
        result += term;
    }

    return fraction(2, 1) * result;
}

fraction fraction::lg(fraction const &epsilon) const
{
    // lg = ln / ln10

    if (_denominator <= 0 || _numerator <= 0) {
        throw std::runtime_error("Logarithm of non-positive number is undefined");
    }

    fraction ln10 = fraction(10, 1).ln(epsilon);
    return this->ln(epsilon) / ln10;
}