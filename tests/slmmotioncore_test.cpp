#include <QtTest>

#include "src/core/motion/slmgesturebinding.h"
#include "src/core/motion/slmspringsolver.h"

class SlmMotionCoreTest : public QObject
{
    Q_OBJECT

private slots:
    void springConverges();
    void gestureSettleDecision();
};

void SlmMotionCoreTest::springConverges()
{
    Slm::Motion::SpringSolver::State s;
    s.value = 0.0;
    s.velocity = 0.0;

    Slm::Motion::SpringParams p;
    const double target = 1.0;

    for (int i = 0; i < 240; ++i) {
        s = Slm::Motion::SpringSolver::step(s, target, p, 1.0 / 120.0);
    }

    QVERIFY2(qAbs(s.value - target) < 0.01, "spring should converge near target");
}

void SlmMotionCoreTest::gestureSettleDecision()
{
    Slm::Motion::GestureBinding g;
    Slm::Motion::GestureSettleConfig cfg;
    cfg.forwardThreshold = 0.5;
    cfg.backwardThreshold = 0.5;
    cfg.velocityThreshold = 800.0;
    g.begin(0.0);
    g.updateByDistance(70.0, 100.0);

    const int forward = g.end(100.0, cfg);
    QCOMPARE(forward, static_cast<int>(Slm::Motion::SettleDecision::Forward));

    g.begin(0.1);
    const int backward = g.end(-1200.0, cfg);
    QCOMPARE(backward, static_cast<int>(Slm::Motion::SettleDecision::Backward));
}

QTEST_MAIN(SlmMotionCoreTest)
#include "slmmotioncore_test.moc"
