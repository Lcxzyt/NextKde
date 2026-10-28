#include "../../shell/native/surface-shape/backdropink.h"
#include <QtTest>

class InkPolicyTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void middleGreyHasOpaqueDarkInk()
    {
        BackdropInkPolicy policy;
        const QByteArray levels = QByteArray::fromHex("0080ff");
        QCOMPARE(policy.update(QSize(3, 1), levels, 0), QByteArray::fromHex("00ffff"));
        QVERIFY(!policy.pending());
    }

    void greyNoiseDoesNotChangeInk()
    {
        BackdropInkPolicy policy;
        policy.update(QSize(1, 1), QByteArray(1, char(140)), 0);
        for (int now = 25; now <= 1000; now += 25) {
            const auto mask = policy.update(QSize(1, 1), QByteArray(1, now % 50 ? char(113) : char(126)), now);
            QCOMPARE(uchar(mask[0]), 255);
            QVERIFY(!policy.pending());
        }
    }

    void singleUpdateFinishesAfterConfirmation()
    {
        BackdropInkPolicy policy;
        policy.update(QSize(1, 1), QByteArray(1, char(255)), 0);
        QCOMPARE(uchar(policy.update(QSize(1, 1), QByteArray(1, 0), 50)[0]), 255);
        QVERIFY(policy.pending());
        QCOMPARE(uchar(policy.update(QSize(1, 1), QByteArray(1, 0), 159)[0]), 255);
        QCOMPARE(uchar(policy.update(QSize(1, 1), QByteArray(1, 0), 160)[0]), 0);
        QVERIFY(!policy.pending());
    }

    void rapidReversalsWaitForQuiet()
    {
        BackdropInkPolicy policy;
        policy.update(QSize(1, 1), QByteArray(1, 0), 0);
        for (int now = 50; now <= 250; now += 50)
            QCOMPARE(uchar(policy.update(QSize(1, 1), QByteArray(1, now % 100 ? char(255) : char(0)), now)[0]), 0);
        QCOMPARE(uchar(policy.update(QSize(1, 1), QByteArray(1, char(255)), 699)[0]), 0);
        QCOMPARE(uchar(policy.update(QSize(1, 1), QByteArray(1, char(255)), 700)[0]), 255);
        QVERIFY(!policy.pending());
    }

    void cellsChangeIndependentlyWithUnchangedMean()
    {
        BackdropInkPolicy policy;
        QCOMPARE(policy.update(QSize(2, 1), QByteArray::fromHex("00ff"), 0), QByteArray::fromHex("00ff"));
        QCOMPARE(policy.update(QSize(2, 1), QByteArray::fromHex("ff00"), 10), QByteArray::fromHex("00ff"));
        QCOMPARE(policy.update(QSize(2, 1), QByteArray::fromHex("ff00"), 120), QByteArray::fromHex("ff00"));
    }

    void resizeAndMalformedInputResetHistory()
    {
        BackdropInkPolicy policy;
        policy.update(QSize(2, 1), QByteArray::fromHex("00ff"), 0);
        QCOMPARE(policy.update(QSize(1, 2), QByteArray::fromHex("ff00"), 1), QByteArray::fromHex("ff00"));
        QVERIFY(policy.update(QSize(64, 48), QByteArray(2, 0), 2).isEmpty());
        QVERIFY(!policy.pending());
        QCOMPARE(policy.update(QSize(1, 1), QByteArray(1, char(128)), 3), QByteArray(1, char(255)));
        policy.clear();
        QCOMPARE(policy.update(QSize(1, 1), QByteArray(1, 0), 4), QByteArray(1, 0));
    }
};

QTEST_APPLESS_MAIN(InkPolicyTest)
#include "ink_policy_test.moc"
