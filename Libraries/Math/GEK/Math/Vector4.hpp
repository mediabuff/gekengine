/// @file
/// @author Todd Zupan <toddzupan@gmail.com>
/// @version $Revision$
/// @section LICENSE
/// https://en.wikipedia.org/wiki/MIT_License
/// @section DESCRIPTION
/// Last Changed: $Date$
#pragma once

#include "GEK/Math/Common.hpp"
#include "GEK/Math/Vector2.hpp"
#include "GEK/Math/Vector3.hpp"

namespace Gek
{
    namespace Math
    {
        template <typename TYPE, typename = typename std::enable_if<std::is_arithmetic<TYPE>::value, TYPE>::type>
        struct Vector4
        {
        public:
            static const Vector4 Zero;
			static const Vector4 One;
			static const Vector4 Black;
			static const Vector4 White;

        public:
            union
            {
				struct { TYPE data[4]; };
                struct
                {
                    union
                    {
                        struct { TYPE x, y, z; };
                        struct { TYPE r, g, b; };
                        Vector3<TYPE> xyz;
                        Vector3<TYPE> rgb;
                    };
                    
                    union
                    {
                        TYPE w;
                        TYPE a;
                    };
                };

                struct
                {
                    Vector2<TYPE> minimum;
                    Vector2<TYPE> maximum;
                };

                struct
                {
                    Vector2<TYPE> xy;
                    Vector2<TYPE> zw;
                };

                struct
                {
                    Vector2<TYPE> position;
                    Vector2<TYPE> size;
                };
            };

        public:
            Vector4(void)
            {
            }

            Vector4(const Vector4<TYPE> &vector)
                : x(vector.x)
                , y(vector.y)
                , z(vector.z)
                , w(vector.w)
            {
            }

            explicit Vector4(TYPE value)
				: x(value)
				, y(value)
				, z(value)
				, w(value)
            {
            }

            explicit Vector4(TYPE x, TYPE y, TYPE z, TYPE w)
				: x(x)
				, y(y)
				, z(z)
				, w(w)
			{
            }

            explicit Vector4(const TYPE *data)
				: x(data[0])
				, y(data[1])
				, z(data[2])
				, w(data[3])
			{
            }

            explicit Vector4(const Vector3<TYPE> &xyz, TYPE w)
				: x(xyz.x)
				, y(xyz.y)
				, z(xyz.z)
				, w(w)
			{
			}

            explicit Vector4(const Vector2<TYPE> &xy, const Vector2<TYPE> &zw)
				: x(xy.x)
				, y(xy.y)
				, z(zw.x)
				, w(zw.y)
			{
			}

            void set(TYPE value)
            {
                x = y = z = w = value;
            }

            void set(TYPE x, TYPE y, TYPE z, TYPE w)
            {
				this->x = x;
				this->y = y;
				this->z = z;
				this->w = w;
            }

            void set(const TYPE *data)
            {
                this->x = data[0];
                this->y = data[1];
                this->z = data[2];
                this->w = data[3];
            }

            void set(const Vector3<TYPE> &xyz, float w)
            {
                this->xyz = xyz;
                this->w = w;
            }

            TYPE getMagnitude(void) const
            {
                return dot(*this);
            }

            TYPE getLength(void) const
            {
                return std::sqrt(getMagnitude());
            }

            TYPE getDistance(const Vector4 &vector) const
            {
                return (vector - (*this)).getLength();
            }

            Vector4<TYPE> getNormal(void) const
            {
                float inverseLength = (1.0f / getLength());
                return ((*this) * inverseLength);
            }

            Vector4<TYPE> getAbsolute(void) const
            {
                return Vector4(
                    std::abs(x),
                    std::abs(y),
                    std::abs(z),
                    std::abs(w));
            }

            Vector4<TYPE> getMinimum(const Vector4 &vector) const
			{
				return Vector4(
					std::min(x, vector.x),
					std::min(y, vector.y),
					std::min(z, vector.z),
					std::min(w, vector.w)
				);
			}

			Vector4<TYPE> getMaximum(const Vector4 &vector) const
			{
				return Vector4(
					std::max(x, vector.x),
					std::max(y, vector.y),
					std::max(z, vector.z),
					std::max(w, vector.w)
				);
			}

			Vector4<TYPE> getClamped(const Vector4 &min, const Vector4 &max) const
			{
				return Vector4(
					std::min(std::max(x, min.x), max.x),
					std::min(std::max(y, min.y), max.y),
					std::min(std::max(z, min.z), max.z),
					std::min(std::max(w, min.w), max.w)
				);
			}

			Vector4<TYPE> getSaturated(void) const
			{
				return getClamped(Zero, One);
			}

			TYPE dot(const Vector4 &vector) const
            {
                return ((x * vector.x) + (y * vector.y) + (z * vector.z) + (w * vector.w));
            }

			void normalize(void)
			{
                float inverseLength = (1.0f / getLength());
                (*this) *= inverseLength;
			}

            std::tuple<TYPE, TYPE, TYPE, TYPE> getTuple(void) const
            {
                return std::make_tuple(x, y, z, w);
            }

            bool operator < (const Vector4 &vector) const
            {
                return (getTuple() < vector.getTuple());
            }

            bool operator > (const Vector4 &vector) const
            {
                return (getTuple() > vector.getTuple());
            }

            bool operator <= (const Vector4 &vector) const
            {
                return (getTuple() <= vector.getTuple());
            }

            bool operator >= (const Vector4 &vector) const
            {
                return (getTuple() >= vector.getTuple());
            }

            bool operator == (const Vector4 &vector) const
            {
                return (getTuple() == vector.getTuple());
            }

            bool operator != (const Vector4 &vector) const
            {
                return (getTuple() != vector.getTuple());
            }

            // vector operations
            float &operator [] (size_t index)
            {
                return data[index];
            }

            float const &operator [] (size_t index) const
            {
                return data[index];
            }

            Vector4<TYPE> &operator = (const Vector4<TYPE> &vector)
            {
                std::tie(x, y, z, w) = vector.getTuple();
                return (*this);
            }

            void operator -= (const Vector4 &vector)
            {
				x -= vector.x;
				y -= vector.y;
				z -= vector.z;
				w -= vector.w;
            }

            void operator += (const Vector4 &vector)
            {
				x += vector.x;
				y += vector.y;
				z += vector.z;
				w += vector.w;
			}

            void operator /= (const Vector4 &vector)
            {
				x /= vector.x;
				y /= vector.y;
				z /= vector.z;
				w /= vector.w;
			}

            void operator *= (const Vector4 &vector)
            {
				x *= vector.x;
				y *= vector.y;
				z *= vector.z;
				w *= vector.w;
			}

            Vector4 operator - (const Vector4 &vector) const
            {
				return Vector4(
					(x - vector.x),
					(y - vector.y),
					(z - vector.z),
					(w - vector.w));
            }

            Vector4 operator + (const Vector4 &vector) const
            {
				return Vector4(
					(x + vector.x),
					(y + vector.y),
					(z + vector.z),
					(w + vector.w));
			}

            Vector4 operator / (const Vector4 &vector) const
            {
				return Vector4(
					(x / vector.x),
					(y / vector.y),
					(z / vector.z),
					(w / vector.w));
			}

            Vector4 operator * (const Vector4 &vector) const
            {
				return Vector4(
					(x * vector.x),
					(y * vector.y),
					(z * vector.z),
					(w * vector.w));
			}

            // scalar operations
            void operator -= (TYPE scalar)
            {
				x += scalar;
				y += scalar;
				z += scalar;
				w += scalar;
            }

            void operator += (TYPE scalar)
            {
				x -= scalar;
				y -= scalar;
				z -= scalar;
				w -= scalar;
			}

            void operator /= (TYPE scalar)
            {
				x /= scalar;
				y /= scalar;
				z /= scalar;
				w /= scalar;
			}

            void operator *= (TYPE scalar)
            {
				x *= scalar;
				y *= scalar;
				z *= scalar;
				w *= scalar;
			}

            Vector4 operator - (TYPE scalar) const
            {
                return Vector4(
                    (x - scalar),
                    (y - scalar),
                    (z - scalar),
                    (w - scalar));
            };

            Vector4 operator + (TYPE scalar) const
            {
                return Vector4(
                    (x + scalar),
                    (y + scalar),
                    (z + scalar),
                    (w + scalar));
			}

            Vector4 operator / (TYPE scalar) const
            {
                return Vector4(
                    (x / scalar),
                    (y / scalar),
                    (z / scalar),
                    (w / scalar));
			}

            Vector4 operator * (TYPE scalar) const
            {
                return Vector4(
                    (x * scalar),
                    (y * scalar),
                    (z * scalar),
                    (w * scalar));
			}
        };

        template <typename TYPE, typename = typename std::enable_if<std::is_arithmetic<TYPE>::value, TYPE>::type>
        Vector4<TYPE> operator - (const Vector4<TYPE> &vector)
        {
			return Vector4<TYPE>(-vector.x, -vector.y, -vector.z, -vector.w);
        }

        template <typename TYPE, typename = typename std::enable_if<std::is_arithmetic<TYPE>::value, TYPE>::type>
        Vector4<TYPE> operator + (TYPE scalar, const Vector4<TYPE> &vector)
        {
			return Vector4<TYPE>(
				(scalar + vector.x),
				(scalar + vector.y),
				(scalar + vector.z),
				(scalar + vector.w));
        }

        template <typename TYPE, typename = typename std::enable_if<std::is_arithmetic<TYPE>::value, TYPE>::type>
        Vector4<TYPE> operator - (TYPE scalar, const Vector4<TYPE> &vector)
        {
			return Vector4<TYPE>(
				(scalar - vector.x),
				(scalar - vector.y),
				(scalar - vector.z),
				(scalar - vector.w));
		}

        template <typename TYPE, typename = typename std::enable_if<std::is_arithmetic<TYPE>::value, TYPE>::type>
        Vector4<TYPE> operator * (TYPE scalar, const Vector4<TYPE> &vector)
        {
			return Vector4<TYPE>(
				(scalar * vector.x),
				(scalar * vector.y),
				(scalar * vector.z),
				(scalar * vector.w));
		}

        template <typename TYPE, typename = typename std::enable_if<std::is_arithmetic<TYPE>::value, TYPE>::type>
        Vector4<TYPE> operator / (TYPE scalar, const Vector4<TYPE> &vector)
        {
			return Vector4<TYPE>(
				(scalar / vector.x),
				(scalar / vector.y),
				(scalar / vector.z),
				(scalar / vector.w));
		}

        using Float4 = Vector4<float>;
        using Int4 = Vector4<int32_t>;
        using UInt4 = Vector4<uint32_t>;
	}; // namespace Math
}; // namespace Gek
