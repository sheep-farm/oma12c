#include <QtTest>
#include <QClipboard>
#include <QGuiApplication>

#include "backend.h"

namespace {
void press(Backend &calc, const QString &keys) {
    const QStringList sequence = keys.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    for (const QString &key : sequence)
        calc.pressKey(key);
}
}

class Oma12cTest : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() {
        QVERIFY(m_settingsDirectory.isValid());
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                           m_settingsDirectory.path());
    }

    void startsAtZero() {
        Backend calc;
        QCOMPARE(calc.display(), QStringLiteral("0.00"));
        QCOMPARE(calc.x(), 0.0);
    }

    void rpnArithmetic() {
        Backend calc;
        press(calc, "2 ENTER 3 +");
        QCOMPARE(calc.x(), 5.0);
        QCOMPARE(calc.display(), QStringLiteral("5.00"));

        press(calc, "4 ×");
        QCOMPARE(calc.x(), 20.0);

        press(calc, "5 ÷");
        QCOMPARE(calc.x(), 4.0);

        press(calc, "2 −");
        QCOMPARE(calc.x(), 2.0);
    }

    void rpnStackManipulation() {
        Backend calc;
        press(calc, "1 ENTER 2 ENTER 3 ENTER");
        QCOMPARE(calc.x(), 3.0);
        QCOMPARE(calc.y(), 2.0);
        QCOMPARE(calc.z(), 1.0);

        press(calc, "R↓");
        QCOMPARE(calc.x(), 2.0);
        QCOMPARE(calc.y(), 1.0);
        QCOMPARE(calc.z(), 0.0);
        QCOMPARE(calc.t(), 3.0);

        press(calc, "x<>y");
        QCOMPARE(calc.x(), 1.0);
        QCOMPARE(calc.y(), 2.0);
    }

    void scientificFormat() {
        Backend calc;
        press(calc, "1 2 3 4 5 6 7 8 9");
        QCOMPARE(calc.display(), QStringLiteral("123456789"));
    }

    void reciprocalAndRoot() {
        Backend calc;
        press(calc, "4 1/x");
        QCOMPARE(calc.x(), 0.25);

        // g y^x is square root.
        press(calc, "4 g y^x");
        QCOMPARE(calc.x(), 2.0);

        // g 1/x is natural exponent.
        press(calc, "0 g 1/x");
        QCOMPARE(calc.x(), 1.0);
    }

    void percentFunctions() {
        Backend calc;
        press(calc, "2 0 0 ENTER 1 0 %");
        QCOMPARE(calc.display(), QStringLiteral("20.00"));

        press(calc, "2 0 0 ENTER 5 0 %T");
        QCOMPARE(calc.display(), QStringLiteral("25.00"));

        press(calc, "5 0 ENTER 2 0 0 Δ%");
        QCOMPARE(calc.display(), QStringLiteral("300.00"));
    }

    void powerAndLogarithm() {
        Backend calc;
        press(calc, "2 ENTER 3 y^x");
        QCOMPARE(calc.x(), 8.0);

        press(calc, "2 LN e^x");
        QCOMPARE(calc.x(), 2.0);
    }

    void storageAndRecall() {
        Backend calc;
        press(calc, "4 2 STO 0");
        press(calc, "RCL 0");
        QCOMPARE(calc.x(), 42.0);

        press(calc, "5 STO 1");
        press(calc, "RCL 0 RCL 1 +");
        QCOMPARE(calc.x(), 47.0);
    }

    void financialTvm() {
        Backend calc;
        // 360 months, 0.5% per month, PV 100000, FV 0, solve PMT.
        press(calc, "3 6 0 n 0 . 5 i 1 0 0 0 0 0 PV 0 FV PMT");
        QVERIFY(calc.x() < 0);
        QVERIFY(calc.x() > -700);

        // Recalculate FV with the solved PMT.
        press(calc, "0 FV");
        QVERIFY(std::abs(calc.x()) < 1.0);
    }

    void clearAndReset() {
        Backend calc;
        press(calc, "1 2 3 ENTER 4 5 6 +");
        QCOMPARE(calc.x(), 579.0);

        press(calc, "ON");
        QCOMPARE(calc.x(), 0.0);
        QCOMPARE(calc.display(), QStringLiteral("0.00"));
    }

    void displayModes() {
        Backend calc;
        press(calc, "1 0 ENTER 3 ÷");
        // Default FIX 2.
        QCOMPARE(calc.display(), QStringLiteral("3.33"));

        press(calc, "FIX 4");
        QCOMPARE(calc.display(), QStringLiteral("3.3333"));
    }

private:
    QTemporaryDir m_settingsDirectory;
};

QTEST_MAIN(Oma12cTest)
#include "tst_oma12c.moc"
