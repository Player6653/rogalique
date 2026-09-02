#pragma once
#include <SFML/System/Vector2.hpp>
#include <cmath>

// 3x3 матрица для 2D-аффинных преобразований в однородных координатах.
// Точки векторы-столбцы: p' = M * p, поэтому в (A * B) сначала применяется B, потом A.
class Matrix3 {
public:
    // По умолчанию единичная матрица.
    Matrix3()
        : m{{1.f, 0.f, 0.f}, {0.f, 1.f, 0.f}, {0.f, 0.f, 1.f}}
    {
    }

    Matrix3(float m00, float m01, float m02, float m10, float m11, float m12, float m20, float m21, float m22)
        : m{{m00, m01, m02}, {m10, m11, m12}, {m20, m21, m22}}
    {
    }

    static Matrix3 identity()
    {
        return Matrix3();
    }

    static Matrix3 translation(sf::Vector2f offset)
    {
        return Matrix3(1.f, 0.f, offset.x, 0.f, 1.f, offset.y, 0.f, 0.f, 1.f);
    }

    // Угол в градусах, против часовой стрелки в математических координатах (y вверх).
    static Matrix3 rotation(float degrees)
    {
        constexpr float kPi = 3.14159265358979323846f;
        float radians = degrees * kPi / 180.f;
        float c = std::cos(radians);
        float s = std::sin(radians);
        return Matrix3(c, -s, 0.f, s, c, 0.f, 0.f, 0.f, 1.f);
    }

    static Matrix3 scale(sf::Vector2f factor)
    {
        return Matrix3(factor.x, 0.f, 0.f, 0.f, factor.y, 0.f, 0.f, 0.f, 1.f);
    }

    // Применяет преобразование к точке; третья координата точки считается равной 1.
    sf::Vector2f transformPoint(sf::Vector2f point) const
    {
        float x = m[0][0] * point.x + m[0][1] * point.y + m[0][2];
        float y = m[1][0] * point.x + m[1][1] * point.y + m[1][2];
        return sf::Vector2f(x, y);
    }

    Matrix3 operator*(const Matrix3& other) const
    {
        Matrix3 result;
        for (int row = 0; row < 3; ++row) {
            for (int col = 0; col < 3; ++col) {
                float sum = 0.f;
                for (int k = 0; k < 3; ++k) {
                    sum += m[row][k] * other.m[k][col];
                }
                result.m[row][col] = sum;
            }
        }
        return result;
    }

    float determinant() const
    {
        return m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0])
               + m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
    }

    // Обратная матрица через метод алгебраических дополнений. Для вырожденной матрицы (det ~ 0) возвращает единичную — безопасный фолбэк вместо деления на ноль.
    Matrix3 inverse() const
    {
        float det = determinant();
        if (std::fabs(det) < 1e-8f) {
            return Matrix3();
        }
        float invDet = 1.f / det;

        Matrix3 result;
        result.m[0][0] = (m[1][1] * m[2][2] - m[1][2] * m[2][1]) * invDet;
        result.m[0][1] = -(m[0][1] * m[2][2] - m[0][2] * m[2][1]) * invDet;
        result.m[0][2] = (m[0][1] * m[1][2] - m[0][2] * m[1][1]) * invDet;

        result.m[1][0] = -(m[1][0] * m[2][2] - m[1][2] * m[2][0]) * invDet;
        result.m[1][1] = (m[0][0] * m[2][2] - m[0][2] * m[2][0]) * invDet;
        result.m[1][2] = -(m[0][0] * m[1][2] - m[0][2] * m[1][0]) * invDet;

        result.m[2][0] = (m[1][0] * m[2][1] - m[1][1] * m[2][0]) * invDet;
        result.m[2][1] = -(m[0][0] * m[2][1] - m[0][1] * m[2][0]) * invDet;
        result.m[2][2] = (m[0][0] * m[1][1] - m[0][1] * m[1][0]) * invDet;

        return result;
    }

    // true, если матрица с точностью до epsilon совпадает с единичной (используется, чтобы проверить, что M * M.inverse() == identity).
    bool isIdentity(float epsilon = 1e-4f) const
    {
        for (int row = 0; row < 3; ++row) {
            for (int col = 0; col < 3; ++col) {
                float expected = (row == col) ? 1.f : 0.f;
                if (std::fabs(m[row][col] - expected) > epsilon) {
                    return false;
                }
            }
        }
        return true;
    }

    float get(int row, int col) const
    {
        return m[row][col];
    }

private:
    float m[3][3];
};
