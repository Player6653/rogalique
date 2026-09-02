#include "pch.h"
#include "Matrix3.h"

TEST(Matrix3Tests, IdentityIsIdentity)
{
    Matrix3 m = Matrix3::identity();
    EXPECT_TRUE(m.isIdentity());
}

TEST(Matrix3Tests, TranslationMovesPoint)
{
    Matrix3 t = Matrix3::translation(sf::Vector2f(10.f, -4.f));
    sf::Vector2f p = t.transformPoint(sf::Vector2f(1.f, 2.f));

    EXPECT_FLOAT_EQ(p.x, 11.f);
    EXPECT_FLOAT_EQ(p.y, -2.f);
}

TEST(Matrix3Tests, ScaleScalesPoint)
{
    Matrix3 s = Matrix3::scale(sf::Vector2f(2.f, 3.f));
    sf::Vector2f p = s.transformPoint(sf::Vector2f(4.f, 5.f));

    EXPECT_FLOAT_EQ(p.x, 8.f);
    EXPECT_FLOAT_EQ(p.y, 15.f);
}

TEST(Matrix3Tests, RotationBy90DegreesRotatesPoint)
{
    Matrix3 r = Matrix3::rotation(90.f);
    sf::Vector2f p = r.transformPoint(sf::Vector2f(1.f, 0.f));

    EXPECT_NEAR(p.x, 0.f, 1e-4f);
    EXPECT_NEAR(p.y, 1.f, 1e-4f);
}

TEST(Matrix3Tests, MultiplyComposesTransformsRightToLeft)
{
    // combo = T * S, поэтому к точке сначала применяется S (масштаб), потом T (перенос).
    Matrix3 t = Matrix3::translation(sf::Vector2f(5.f, 0.f));
    Matrix3 s = Matrix3::scale(sf::Vector2f(2.f, 2.f));
    Matrix3 combo = t * s;

    sf::Vector2f p = combo.transformPoint(sf::Vector2f(1.f, 1.f));

    EXPECT_FLOAT_EQ(p.x, 7.f);
    EXPECT_FLOAT_EQ(p.y, 2.f);
}

TEST(Matrix3Tests, MultiplyByInverseGivesIdentity)
{
    Matrix3 t = Matrix3::translation(sf::Vector2f(10.f, -5.f));
    Matrix3 r = Matrix3::rotation(37.f);
    Matrix3 s = Matrix3::scale(sf::Vector2f(2.f, 3.f));

    Matrix3 combo = t * r * s;
    Matrix3 inverse = combo.inverse();

    EXPECT_TRUE((combo * inverse).isIdentity());
    EXPECT_TRUE((inverse * combo).isIdentity());
}

TEST(Matrix3Tests, InverseUndoesTransformOnPoint)
{
    Matrix3 t = Matrix3::translation(sf::Vector2f(10.f, -5.f));
    Matrix3 r = Matrix3::rotation(37.f);
    Matrix3 s = Matrix3::scale(sf::Vector2f(2.f, 3.f));
    Matrix3 combo = t * r * s;

    sf::Vector2f original(4.f, 7.f);
    sf::Vector2f transformed = combo.transformPoint(original);
    sf::Vector2f restored = combo.inverse().transformPoint(transformed);

    EXPECT_NEAR(restored.x, original.x, 1e-3f);
    EXPECT_NEAR(restored.y, original.y, 1e-3f);
}
