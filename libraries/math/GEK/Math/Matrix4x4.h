#pragma once

#include <algorithm>
#include <xmmintrin.h>
#include "GEK\Math\Common.h"
#include "GEK\Math\Vector4.h"

namespace Gek
{
    namespace Math
    {
        struct Quaternion;

        struct Float4x4
        {
        public:
            union
            {
                struct { float data[16]; };
                struct { float table[4][4]; };
                struct { Float4 rows[4]; };
                struct { __m128 simd[4]; };

                struct
                {
                    float _11, _12, _13, _14;
                    float _21, _22, _23, _24;
                    float _31, _32, _33, _34;
                    float _41, _42, _43, _44;
                };

                struct
                {
                    Float4 rx;
                    Float4 ry;
                    Float4 rz;
                    union
                    {
                        struct
                        {
                            Float4 rw;
                        };

                        struct
                        {
                            Float3 translation;
                            float tw;
                        };
                    };
                };

                struct
                {
                    struct { Float3 nx; float nxw; };
                    struct { Float3 ny; float nyw; };
                    struct { Float3 nz; float nzw; };
                    struct
                    {
                        Float3 translation;
                        float tw;
                    };
                };
            };

        public:
            Float4x4(void)
            {
                setIdentity();
            }

            Float4x4(const __m128(&data)[4])
                : simd{ data[0], data[1], data[2], data[3] }
            {
            }

            Float4x4(const float(&data)[16])
                : data{ data[ 0], data[ 1], data[ 2], data[ 3],
                        data[ 4], data[ 5], data[ 6], data[ 7],
                        data[ 8], data[ 9], data[10], data[11],
                        data[12], data[13], data[14], data[15] }
            {
            }

            Float4x4(const float *data)
                : data{ data[ 0], data[ 1], data[ 2], data[ 3],
                        data[ 4], data[ 5], data[ 6], data[ 7],
                        data[ 8], data[ 9], data[10], data[11],
                        data[12], data[13], data[14], data[15] }
            {
            }

            Float4x4(const Float4x4 &matrix)
                : simd{ matrix.simd[0], matrix.simd[1], matrix.simd[2], matrix.simd[3] }
            {
            }

            Float4x4(const Float3 &euler)
            {
                setEuler(euler.x, euler.y, euler.z);
            }

            Float4x4(float x, float y, float z)
            {
                setEuler(x, y, z);
            }

            Float4x4(const Float3 &axis, float radians)
            {
                setRotation(axis, radians);
            }

            Float4x4(const Quaternion &rotation)
            {
                setRotation(rotation);
            }

            Float4x4(const Quaternion &rotation, const Float3 &translation)
            {
                setRotation(rotation, translation);
            }

            void setZero(void)
            {
                rows[0].set(0.0f, 0.0f, 0.0f, 0.0f);
                rows[1].set(0.0f, 0.0f, 0.0f, 0.0f);
                rows[2].set(0.0f, 0.0f, 0.0f, 0.0f);
                rows[3].set(0.0f, 0.0f, 0.0f, 0.0f);
            }

            void setIdentity(void)
            {
                rows[0].set(1.0f, 0.0f, 0.0f, 0.0f);
                rows[1].set(0.0f, 1.0f, 0.0f, 0.0f);
                rows[2].set(0.0f, 0.0f, 1.0f, 0.0f);
                rows[3].set(0.0f, 0.0f, 0.0f, 1.0f);
            }

            void setScaling(float scalar)
            {
                setScaling(Float3(scalar));
            }

            void setScaling(const Float3 &vector)
            {
                _11 = vector.x;
                _22 = vector.y;
                _33 = vector.z;
            }

            void setEuler(const Float3 &euler)
            {
                setEuler(euler.x, euler.y, euler.z);
            }

            void setEuler(float x, float y, float z)
            {
                float cosX(std::cos(x));
                float sinX(std::sin(x));
                float cosY(std::cos(y));
                float sinY(std::sin(y));
                float cosZ(std::cos(z));
                float sinZ(std::sin(z));
                float cosXsinY(cosX * sinY);
                float sinXsinY(sinX * sinY);

                table[0][0] = ( cosY * cosZ);
                table[1][0] = (-cosY * sinZ);
                table[2][0] =  sinY;
                table[3][0] = 0.0f;

                table[0][1] = ( sinXsinY * cosZ + cosX * sinZ);
                table[1][1] = (-sinXsinY * sinZ + cosX * cosZ);
                table[2][1] = (-sinX * cosY);
                table[3][1] = 0.0f;

                table[0][2] = (-cosXsinY * cosZ + sinX * sinZ);
                table[1][2] = ( cosXsinY * sinZ + sinX * cosZ);
                table[2][2] = ( cosX * cosY);
                table[3][2] = 0.0f;

                table[0][3] = 0.0f;
                table[1][3] = 0.0f;
                table[2][3] = 0.0f;
                table[3][3] = 1.0f;
            }

            void setRotation(const Float3 &axis, float radians)
            {
                float cosAngle(std::cos(radians));
                float sinAngle(std::sin(radians));

                table[0][0] = (cosAngle + axis.x * axis.x * (1.0f - cosAngle));
                table[0][1] = ( axis.z * sinAngle + axis.y * axis.x * (1.0f - cosAngle));
                table[0][2] = (-axis.y * sinAngle + axis.z * axis.x * (1.0f - cosAngle));
                table[0][3] = 0.0f;

                table[1][0] = (-axis.z * sinAngle + axis.x * axis.y * (1.0f - cosAngle));
                table[1][1] = (cosAngle + axis.y * axis.y * (1.0f - cosAngle));
                table[1][2] = ( axis.x * sinAngle + axis.z * axis.y * (1.0f - cosAngle));
                table[1][3] = 0.0f;

                table[2][0] = ( axis.y * sinAngle + axis.x * axis.z * (1.0f - cosAngle));
                table[2][1] = (-axis.x * sinAngle + axis.y * axis.z * (1.0f - cosAngle));
                table[2][2] = (cosAngle + axis.z * axis.z * (1.0f - cosAngle));
                table[2][3] = 0.0f;

                table[3][0] = 0.0f;
                table[3][1] = 0.0f;
                table[3][2] = 0.0f;
                table[3][3] = 1.0f;
            }

            void setRotation(const Quaternion &rotation);

            void setRotation(const Quaternion &rotation, const Float3 &translation);

            void setRotationX(float radians)
            {
                float cosAngle(std::cos(radians));
                float sinAngle(std::sin(radians));
                table[0][0] = 1.0f; table[0][1] = 0.0f;   table[0][2] = 0.0f;  table[0][3] = 0.0f;
                table[1][0] = 0.0f; table[1][1] = cosAngle;  table[1][2] = sinAngle; table[1][3] = 0.0f;
                table[2][0] = 0.0f; table[2][1] = -sinAngle; table[2][2] = cosAngle; table[2][3] = 0.0f;
                table[3][0] = 0.0f; table[3][1] = 0.0f;   table[3][2] = 0.0f;  table[3][3] = 1.0f;
            }

            void setRotationY(float radians)
            {
                float cosAngle(std::cos(radians));
                float sinAngle(std::sin(radians));
                table[0][0] = cosAngle; table[0][1] = 0.0f; table[0][2] = -sinAngle; table[0][3] = 0.0f;
                table[1][0] = 0.0f;  table[1][1] = 1.0f; table[1][2] = 0.0f;   table[1][3] = 0.0f;
                table[2][0] = sinAngle; table[2][1] = 0.0f; table[2][2] = cosAngle;  table[2][3] = 0.0f;
                table[3][0] = 0.0f;  table[3][1] = 0.0f; table[3][2] = 0.0f;   table[3][3] = 1.0f;
            }

            void setRotationZ(float radians)
            {
                float cosAngle(std::cos(radians));
                float sinAngle(std::sin(radians));
                table[0][0] = cosAngle; table[0][1] = sinAngle; table[0][2] = 0.0f; table[0][3] = 0.0f;
                table[1][0] =-sinAngle; table[1][1] = cosAngle; table[1][2] = 0.0f; table[1][3] = 0.0f;
                table[2][0] = 0.0f;  table[2][1] = 0.0f;  table[2][2] = 1.0f; table[2][3] = 0.0f;
                table[3][0] = 0.0f;  table[3][1] = 0.0f;  table[3][2] = 0.0f; table[3][3] = 1.0f;
            }

            void setOrthographic(float left, float top, float right, float bottom, float nearDepth, float farDepth)
            {
                table[0][0] = (2.0f / (right - left));
                table[1][0] = 0.0f;
                table[2][0] = 0.0f;
                table[3][0] = -((right + left) / (right - left));;

                table[0][1] = 0.0f;
                table[1][1] = (2.0f / (top - bottom));
                table[2][1] = 0.0f;
                table[3][1] = -((top + bottom) / (top - bottom));

                table[0][2] = 0.0f;
                table[1][2] = 0.0f;
                table[2][2] = (-2.0f / (farDepth - nearDepth));
                table[3][2] = -((farDepth + nearDepth) / (farDepth - nearDepth));

                table[0][3] = 0.0f;
                table[1][3] = 0.0f;
                table[2][3] = 0.0f;
                table[3][3] = 1.0f;
            }

            void setPerspective(float fieldOfView, float aspectRatio, float nearDepth, float farDepth)
            {
                float x(1.0f / std::tan(fieldOfView * 0.5f));
                float y(x * aspectRatio);
                float distance(farDepth - nearDepth);

                table[0][0] = x;
                table[0][1] = 0.0f;
                table[0][2] = 0.0f;
                table[0][3] = 0.0f;

                table[1][0] = 0.0f;
                table[1][1] = y;
                table[1][2] = 0.0f;
                table[1][3] = 0.0f;

                table[2][0] = 0.0f;
                table[2][1] = 0.0f;
                table[2][2] = ((farDepth + nearDepth) / distance);
                table[2][3] = 1.0f;

                table[3][0] = 0.0f;
                table[3][1] = 0.0f;
                table[3][2] = -((2.0f * farDepth * nearDepth) / distance);
                table[3][3] = 0.0f;
            }

            void setLookAt(const Float3 &source, const Float3 &target, const Float3 &worldUpVector)
            {
                nz = ((target - source).getNormal());
                nx = (worldUpVector.cross(nz).getNormal());
                ny = (nz.cross(nx).getNormal());
                nxw = 0.0f;
                nyw = 0.0f;
                nzw = 0.0f;

                rw.set(0.0f, 0.0f, 0.0f, 1.0f);

                invert();
            }

            void setLookAt(const Float3 &direction, const Float3 &worldUpVector)
            {
                nz = (direction.getNormal());
                nx = (worldUpVector.cross(nz).getNormal());
                ny = (nz.cross(nx).getNormal());
                nxw = 0.0f;
                nyw = 0.0f;
                nzw = 0.0f;

                rw.set(0.0f, 0.0f, 0.0f, 1.0f);

                invert();
            }

            Float3 getEuler(void) const
            {
                Float3 euler;
                euler.y = std::asin(_31);

                float cosAngle = std::cos(euler.y);
                if (std::abs(cosAngle) > 0.005)
                {
                    euler.x = std::atan2(-(_32 / cosAngle), (_33 / cosAngle));
                    euler.z = std::atan2(-(_21 / cosAngle), (_11 / cosAngle));
                }
                else
                {
                    euler.x = 0.0f;
                    euler.y = std::atan2(_12, _22);
                }

                if (euler.x < 0.0f)
                {
                    euler.x += (Pi * 2.0f);
                }

                if (euler.y < 0.0f)
                {
                    euler.y += (Pi * 2.0f);
                }

                if (euler.z < 0.0f)
                {
                    euler.z += (Pi * 2.0f);
                }

                return euler;
            }

            Float3 getScaling(void) const
            {
                return Float3(_11, _22, _33);
            }

            float getDeterminant(void) const
            {
                return ((table[0][0] * table[1][1] - table[1][0] * table[0][1]) *
                        (table[2][2] * table[3][3] - table[3][2] * table[2][3]) -
                        (table[0][0] * table[2][1] - table[2][0] * table[0][1]) *
                        (table[1][2] * table[3][3] - table[3][2] * table[1][3]) +
                        (table[0][0] * table[3][1] - table[3][0] * table[0][1]) *
                        (table[1][2] * table[2][3] - table[2][2] * table[1][3]) +
                        (table[1][0] * table[2][1] - table[2][0] * table[1][1]) *
                        (table[0][2] * table[3][3] - table[3][2] * table[0][3]) -
                        (table[1][0] * table[3][1] - table[3][0] * table[1][1]) *
                        (table[0][2] * table[2][3] - table[2][2] * table[0][3]) +
                        (table[2][0] * table[3][1] - table[3][0] * table[2][1]) *
                        (table[0][2] * table[1][3] - table[1][2] * table[0][3]));
            }

            Float4x4 getTranspose(void) const
            {
                return Float4x4({ _11, _21, _31, _41,
                                  _12, _22, _32, _42,
                                  _13, _23, _33, _43, 
                                  _14, _24, _34, _44 });
            }

            Float4x4 getInverse(void) const
            {
                float determinant(getDeterminant());
                if (std::abs(determinant) < Epsilon)
                {
                    return Float4x4();
                }
                else
                {
                    determinant = (1.0f / determinant);

                    Float4x4 matrix;
                    matrix.table[0][0] = (determinant * (table[1][1] * (table[2][2] * table[3][3] - table[3][2] * table[2][3]) + table[2][1] * (table[3][2] * table[1][3] - table[1][2] * table[3][3]) + table[3][1] * (table[1][2] * table[2][3] - table[2][2] * table[1][3])));
                    matrix.table[1][0] = (determinant * (table[1][2] * (table[2][0] * table[3][3] - table[3][0] * table[2][3]) + table[2][2] * (table[3][0] * table[1][3] - table[1][0] * table[3][3]) + table[3][2] * (table[1][0] * table[2][3] - table[2][0] * table[1][3])));
                    matrix.table[2][0] = (determinant * (table[1][3] * (table[2][0] * table[3][1] - table[3][0] * table[2][1]) + table[2][3] * (table[3][0] * table[1][1] - table[1][0] * table[3][1]) + table[3][3] * (table[1][0] * table[2][1] - table[2][0] * table[1][1])));
                    matrix.table[3][0] = (determinant * (table[1][0] * (table[3][1] * table[2][2] - table[2][1] * table[3][2]) + table[2][0] * (table[1][1] * table[3][2] - table[3][1] * table[1][2]) + table[3][0] * (table[2][1] * table[1][2] - table[1][1] * table[2][2])));
                    matrix.table[0][1] = (determinant * (table[2][1] * (table[0][2] * table[3][3] - table[3][2] * table[0][3]) + table[3][1] * (table[2][2] * table[0][3] - table[0][2] * table[2][3]) + table[0][1] * (table[3][2] * table[2][3] - table[2][2] * table[3][3])));
                    matrix.table[1][1] = (determinant * (table[2][2] * (table[0][0] * table[3][3] - table[3][0] * table[0][3]) + table[3][2] * (table[2][0] * table[0][3] - table[0][0] * table[2][3]) + table[0][2] * (table[3][0] * table[2][3] - table[2][0] * table[3][3])));
                    matrix.table[2][1] = (determinant * (table[2][3] * (table[0][0] * table[3][1] - table[3][0] * table[0][1]) + table[3][3] * (table[2][0] * table[0][1] - table[0][0] * table[2][1]) + table[0][3] * (table[3][0] * table[2][1] - table[2][0] * table[3][1])));
                    matrix.table[3][1] = (determinant * (table[2][0] * (table[3][1] * table[0][2] - table[0][1] * table[3][2]) + table[3][0] * (table[0][1] * table[2][2] - table[2][1] * table[0][2]) + table[0][0] * (table[2][1] * table[3][2] - table[3][1] * table[2][2])));
                    matrix.table[0][2] = (determinant * (table[3][1] * (table[0][2] * table[1][3] - table[1][2] * table[0][3]) + table[0][1] * (table[1][2] * table[3][3] - table[3][2] * table[1][3]) + table[1][1] * (table[3][2] * table[0][3] - table[0][2] * table[3][3])));
                    matrix.table[1][2] = (determinant * (table[3][2] * (table[0][0] * table[1][3] - table[1][0] * table[0][3]) + table[0][2] * (table[1][0] * table[3][3] - table[3][0] * table[1][3]) + table[1][2] * (table[3][0] * table[0][3] - table[0][0] * table[3][3])));
                    matrix.table[2][2] = (determinant * (table[3][3] * (table[0][0] * table[1][1] - table[1][0] * table[0][1]) + table[0][3] * (table[1][0] * table[3][1] - table[3][0] * table[1][1]) + table[1][3] * (table[3][0] * table[0][1] - table[0][0] * table[3][1])));
                    matrix.table[3][2] = (determinant * (table[3][0] * (table[1][1] * table[0][2] - table[0][1] * table[1][2]) + table[0][0] * (table[3][1] * table[1][2] - table[1][1] * table[3][2]) + table[1][0] * (table[0][1] * table[3][2] - table[3][1] * table[0][2])));
                    matrix.table[0][3] = (determinant * (table[0][1] * (table[2][2] * table[1][3] - table[1][2] * table[2][3]) + table[1][1] * (table[0][2] * table[2][3] - table[2][2] * table[0][3]) + table[2][1] * (table[1][2] * table[0][3] - table[0][2] * table[1][3])));
                    matrix.table[1][3] = (determinant * (table[0][2] * (table[2][0] * table[1][3] - table[1][0] * table[2][3]) + table[1][2] * (table[0][0] * table[2][3] - table[2][0] * table[0][3]) + table[2][2] * (table[1][0] * table[0][3] - table[0][0] * table[1][3])));
                    matrix.table[2][3] = (determinant * (table[0][3] * (table[2][0] * table[1][1] - table[1][0] * table[2][1]) + table[1][3] * (table[0][0] * table[2][1] - table[2][0] * table[0][1]) + table[2][3] * (table[1][0] * table[0][1] - table[0][0] * table[1][1])));
                    matrix.table[3][3] = (determinant * (table[0][0] * (table[1][1] * table[2][2] - table[2][1] * table[1][2]) + table[1][0] * (table[2][1] * table[0][2] - table[0][1] * table[2][2]) + table[2][0] * (table[0][1] * table[1][2] - table[1][1] * table[0][2])));
                    return matrix;
                }
            }

            Float4x4 getRotation(void) const
            {
                return Float4x4({ _11, _12, _13, 0,
                                  _21, _22, _23, 0,
                                  _31, _32, _33, 0,
                                    0,   0,   0, 1 });
            }

            void transpose(void)
            {
                (*this) = getTranspose();
            }

            void invert(void)
            {
                (*this) = getInverse();
            }

            Float4 operator [] (int row) const
            {
                return rows[row];
            }

            Float4 &operator [] (int row)
            {
                return rows[row];
            }

            operator const float *() const
            {
                return data;
            }

            operator float *()
            {
                return data;
            }

            void operator *= (const Float4x4 &matrix)
            {
                simd[0] = _mm_add_ps(_mm_add_ps(_mm_mul_ps(_mm_shuffle_ps(simd[0], simd[0], _MM_SHUFFLE(0, 0, 0, 0)), matrix.simd[0]),
                                                _mm_mul_ps(_mm_shuffle_ps(simd[0], simd[0], _MM_SHUFFLE(1, 1, 1, 1)), matrix.simd[1])),
                                     _mm_add_ps(_mm_mul_ps(_mm_shuffle_ps(simd[0], simd[0], _MM_SHUFFLE(2, 2, 2, 2)), matrix.simd[2]),
                                                _mm_mul_ps(_mm_shuffle_ps(simd[0], simd[0], _MM_SHUFFLE(3, 3, 3, 3)), matrix.simd[3])));
                simd[1] = _mm_add_ps(_mm_add_ps(_mm_mul_ps(_mm_shuffle_ps(simd[1], simd[1], _MM_SHUFFLE(0, 0, 0, 0)), matrix.simd[0]),
                                                _mm_mul_ps(_mm_shuffle_ps(simd[1], simd[1], _MM_SHUFFLE(1, 1, 1, 1)), matrix.simd[1])),
                                     _mm_add_ps(_mm_mul_ps(_mm_shuffle_ps(simd[1], simd[1], _MM_SHUFFLE(2, 2, 2, 2)), matrix.simd[2]),
                                                _mm_mul_ps(_mm_shuffle_ps(simd[1], simd[1], _MM_SHUFFLE(3, 3, 3, 3)), matrix.simd[3])));
                simd[2] = _mm_add_ps(_mm_add_ps(_mm_mul_ps(_mm_shuffle_ps(simd[2], simd[2], _MM_SHUFFLE(0, 0, 0, 0)), matrix.simd[0]),
                                                _mm_mul_ps(_mm_shuffle_ps(simd[2], simd[2], _MM_SHUFFLE(1, 1, 1, 1)), matrix.simd[1])),
                                     _mm_add_ps(_mm_mul_ps(_mm_shuffle_ps(simd[2], simd[2], _MM_SHUFFLE(2, 2, 2, 2)), matrix.simd[2]),
                                                _mm_mul_ps(_mm_shuffle_ps(simd[2], simd[2], _MM_SHUFFLE(3, 3, 3, 3)), matrix.simd[3])));
                simd[3] = _mm_add_ps(_mm_add_ps(_mm_mul_ps(_mm_shuffle_ps(simd[3], simd[3], _MM_SHUFFLE(0, 0, 0, 0)), matrix.simd[0]),
                                                _mm_mul_ps(_mm_shuffle_ps(simd[3], simd[3], _MM_SHUFFLE(1, 1, 1, 1)), matrix.simd[1])),
                                     _mm_add_ps(_mm_mul_ps(_mm_shuffle_ps(simd[3], simd[3], _MM_SHUFFLE(2, 2, 2, 2)), matrix.simd[2]),
                                                _mm_mul_ps(_mm_shuffle_ps(simd[3], simd[3], _MM_SHUFFLE(3, 3, 3, 3)), matrix.simd[3])));
            }

            Float4x4 operator * (const Float4x4 &matrix) const
            {
                return Float4x4({ _mm_add_ps(_mm_add_ps(_mm_mul_ps(_mm_shuffle_ps(simd[0], simd[0], _MM_SHUFFLE(0, 0, 0, 0)), matrix.simd[0]),
                                             _mm_mul_ps(_mm_shuffle_ps(simd[0], simd[0], _MM_SHUFFLE(1, 1, 1, 1)), matrix.simd[1])),
                                  _mm_add_ps(_mm_mul_ps(_mm_shuffle_ps(simd[0], simd[0], _MM_SHUFFLE(2, 2, 2, 2)), matrix.simd[2]),
                                             _mm_mul_ps(_mm_shuffle_ps(simd[0], simd[0], _MM_SHUFFLE(3, 3, 3, 3)), matrix.simd[3]))),
                                  _mm_add_ps(_mm_add_ps(_mm_mul_ps(_mm_shuffle_ps(simd[1], simd[1], _MM_SHUFFLE(0, 0, 0, 0)), matrix.simd[0]),
                                             _mm_mul_ps(_mm_shuffle_ps(simd[1], simd[1], _MM_SHUFFLE(1, 1, 1, 1)), matrix.simd[1])),
                                  _mm_add_ps(_mm_mul_ps(_mm_shuffle_ps(simd[1], simd[1], _MM_SHUFFLE(2, 2, 2, 2)), matrix.simd[2]),
                                             _mm_mul_ps(_mm_shuffle_ps(simd[1], simd[1], _MM_SHUFFLE(3, 3, 3, 3)), matrix.simd[3]))),
                                  _mm_add_ps(_mm_add_ps(_mm_mul_ps(_mm_shuffle_ps(simd[2], simd[2], _MM_SHUFFLE(0, 0, 0, 0)), matrix.simd[0]),
                                             _mm_mul_ps(_mm_shuffle_ps(simd[2], simd[2], _MM_SHUFFLE(1, 1, 1, 1)), matrix.simd[1])),
                                  _mm_add_ps(_mm_mul_ps(_mm_shuffle_ps(simd[2], simd[2], _MM_SHUFFLE(2, 2, 2, 2)), matrix.simd[2]),
                                             _mm_mul_ps(_mm_shuffle_ps(simd[2], simd[2], _MM_SHUFFLE(3, 3, 3, 3)), matrix.simd[3]))),
                                  _mm_add_ps(_mm_add_ps(_mm_mul_ps(_mm_shuffle_ps(simd[3], simd[3], _MM_SHUFFLE(0, 0, 0, 0)), matrix.simd[0]),
                                             _mm_mul_ps(_mm_shuffle_ps(simd[3], simd[3], _MM_SHUFFLE(1, 1, 1, 1)), matrix.simd[1])),
                                  _mm_add_ps(_mm_mul_ps(_mm_shuffle_ps(simd[3], simd[3], _MM_SHUFFLE(2, 2, 2, 2)), matrix.simd[2]),
                                             _mm_mul_ps(_mm_shuffle_ps(simd[3], simd[3], _MM_SHUFFLE(3, 3, 3, 3)), matrix.simd[3]))) });
            }

            Float4x4 operator = (const Float4x4 &matrix)
            {
                rows[0] = matrix.rows[0];
                rows[1] = matrix.rows[1];
                rows[2] = matrix.rows[2];
                rows[3] = matrix.rows[3];
                return (*this);
            }

            Float4x4 operator = (const Quaternion &rotation)
            {
                setRotation(rotation);
                return (*this);
            }

            Float3 operator * (const Float3 &vector) const
            {
                return Float3(((vector.x * _11) + (vector.y * _21) + (vector.z * _31)) + _41,
                              ((vector.x * _12) + (vector.y * _22) + (vector.z * _32)) + _42,
                              ((vector.x * _13) + (vector.y * _23) + (vector.z * _33)) + _43);
            }

            Float4 operator * (const Float4 &vector) const
            {
                return Float4(((vector.x * _11) + (vector.y * _21) + (vector.z * _31) + (vector.w * _41)),
                              ((vector.x * _12) + (vector.y * _22) + (vector.z * _32) + (vector.w * _42)),
                              ((vector.x * _13) + (vector.y * _23) + (vector.z * _33) + (vector.w * _43)),
                              ((vector.x * _14) + (vector.y * _24) + (vector.z * _34) + (vector.w * _44)));
            }

            Float4x4 operator * (float nScalar) const
            {
                return Float4x4({ (_11 * nScalar), (_12 * nScalar), (_13 * nScalar), (_14 * nScalar),
                                  (_21 * nScalar), (_22 * nScalar), (_23 * nScalar), (_24 * nScalar),
                                  (_31 * nScalar), (_32 * nScalar), (_33 * nScalar), (_34 * nScalar),
                                  (_41 * nScalar), (_42 * nScalar), (_43 * nScalar), (_44 * nScalar) });
            }

            Float4x4 operator + (const Float4x4 &matrix) const
            {
                return Float4x4({ _11 + matrix._11, _12 + matrix._12, _13 + matrix._13, _14 + matrix._14,
                                  _21 + matrix._21, _22 + matrix._22, _23 + matrix._23, _24 + matrix._24,
                                  _31 + matrix._31, _32 + matrix._32, _33 + matrix._33, _34 + matrix._34,
                                  _41 + matrix._41, _42 + matrix._42, _43 + matrix._43, _44 + matrix._44 });
            }

            void operator += (const Float4x4 &matrix)
            {
                _11 += matrix._11; _12 += matrix._12; _13 += matrix._13; _14 += matrix._14;
                _21 += matrix._21; _22 += matrix._22; _23 += matrix._23; _24 += matrix._24;
                _31 += matrix._31; _32 += matrix._32; _33 += matrix._33; _34 += matrix._34;
                _41 += matrix._41; _42 += matrix._42; _43 += matrix._43; _44 += matrix._44;
            }
        };
    }; // namespace Math
}; // namespace Gek

#include "GEK\Math\Quaternion.h"

namespace Gek
{
    namespace Math
    {
        void Float4x4::setRotation(const Quaternion &rotation)
        {
            setRotation(rotation, Float3(0.0f, 0.0f, 0.0f));
        }

        void Float4x4::setRotation(const Quaternion &rotation, const Float3 &translation)
        {
            float xy(rotation.x * rotation.y);
            float zw(rotation.z * rotation.w);
            float xz(rotation.x * rotation.z);
            float yw(rotation.y * rotation.w);
            float yz(rotation.y * rotation.z);
            float xw(rotation.x * rotation.w);
            float squareX(rotation.x * rotation.x);
            float squareY(rotation.y * rotation.y);
            float squareZ(rotation.z * rotation.z);
            float squareW(rotation.w * rotation.w);
            float determinant(1.0f / (squareX + squareY + squareZ + squareW));
            rows[0].set( ((squareX - squareY - squareZ + squareW) * determinant), (2.0f * (xy + zw) * determinant), (2.0f * (xz - yw) * determinant), 0.0f);
            rows[1].set((2.0f * (xy - zw) * determinant), ((-squareX + squareY - squareZ + squareW) * determinant), (2.0f * (yz + xw) * determinant), 0.0f);
            rows[2].set((2.0f * (xz + yw) * determinant), (2.0f * (yz - xw) * determinant), ((-squareX - squareY + squareZ + squareW) * determinant), 0.0f);
            this->translation = translation;
            tw = 1.0f;
        }
    }; // namespace Math
}; // namespace Gek
