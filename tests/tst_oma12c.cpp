#include <QtTest>
#include <QClipboard>
#include <QGuiApplication>

#include "backend.h"

#include <cmath>

namespace {
void press(Backend &calc, const QString &keys) {
    const QStringList sequence = keys.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    for (const QString &key : sequence)
        calc.pressKey(key);
}

bool near(double a, double b, double eps = 1e-9) {
    return std::abs(a - b) < eps;
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

        press(calc, "3 y^x");
        QCOMPARE(calc.x(), 8.0);

        press(calc, "4 g y^x");   // sqrt
        QCOMPARE(calc.x(), 2.0);

        press(calc, "2 1/x");
        QCOMPARE(calc.x(), 0.5);

        press(calc, "4 x^2");
        QCOMPARE(calc.x(), 16.0);
    }

    void rpnStackManipulation() {
        Backend calc;
        press(calc, "1 ENTER 2 ENTER 3 ENTER");
        QCOMPARE(calc.x(), 3.0);
        QCOMPARE(calc.y(), 2.0);
        QCOMPARE(calc.z(), 1.0);
        QCOMPARE(calc.t(), 0.0);

        press(calc, "R↓");
        QCOMPARE(calc.x(), 2.0);
        QCOMPARE(calc.y(), 1.0);
        QCOMPARE(calc.z(), 0.0);
        QCOMPARE(calc.t(), 3.0);

        press(calc, "x<>y");
        QCOMPARE(calc.x(), 1.0);
        QCOMPARE(calc.y(), 2.0);

        press(calc, "5 ×");
        QCOMPARE(calc.x(), 5.0);
        QCOMPARE(calc.lastX(), 5.0);

        press(calc, "g CLx");   // LSTx
        QCOMPARE(calc.x(), 5.0);
    }

    void clearAndReset() {
        Backend calc;
        press(calc, "1 2 3 ENTER 4 5 6 +");
        QCOMPARE(calc.x(), 579.0);

        press(calc, "CLx");
        QCOMPARE(calc.x(), 0.0);

        press(calc, "1 2 3");
        press(calc, "ON");
        QCOMPARE(calc.x(), 0.0);
        QCOMPARE(calc.display(), QStringLiteral("0.00"));
    }

    void displayModes() {
        Backend calc;
        press(calc, "1 0 ENTER 3 ÷");
        QCOMPARE(calc.display(), QStringLiteral("3.33"));

        press(calc, "FIX 4");
        QCOMPARE(calc.display(), QStringLiteral("3.3333"));

        press(calc, "f .");   // SCI
        QVERIFY(calc.display().toUpper().contains(QStringLiteral("3.3333")));

        press(calc, "g .");   // ENG
        QVERIFY(calc.display().toUpper().contains(QStringLiteral("3.333")));
    }

    void scientificFormat() {
        Backend calc;
        press(calc, "1 2 3 4 5 6 7 8 9");
        QCOMPARE(calc.display(), QStringLiteral("123456789"));

        press(calc, "1 2 3 4 5 6 7 8 9 0");
        // HP-12C rounds 10 significant digits -> 1234567891 in FIX or 1.23457E+09 in SCI.
        QVERIFY(calc.display().toUpper().startsWith(QStringLiteral("1.23457E")) ||
                calc.display() == QStringLiteral("1234567891"));
    }

    void hp12cPrecision() {
        Backend calc;
        // HP-12C BCD: 1/3 = 0.3333333333, then * 3 = 0.9999999999
        press(calc, "1 ENTER 3 ÷");
        QVERIFY(near(calc.x(), 0.3333333333, 1e-10));

        press(calc, "3 ×");
        QVERIFY(near(calc.x(), 0.9999999999, 1e-10));
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

        press(calc, "3 g 1/x");   // e^x
        QVERIFY(near(calc.x(), std::exp(3.0), 1e-7));

        press(calc, "0 g 1/x");
        QCOMPARE(calc.x(), 1.0);

        press(calc, "5 g y^x");   // square root
        QVERIFY(near(calc.x(), std::sqrt(5.0), 1e-9));
    }

    void factorialAndParts() {
        Backend calc;
        press(calc, "5 g 3");   // n!
        QCOMPARE(calc.x(), 120.0);

        press(calc, "1 2 . 3 4 5 g %");   // INTG
        QCOMPARE(calc.x(), 12.0);

        press(calc, "1 2 . 3 4 5 g Δ%");   // FRAC
        QVERIFY(near(calc.x(), 0.345, 1e-9));
    }

    void storageAndRecall() {
        Backend calc;
        press(calc, "4 2 STO 0");
        press(calc, "RCL 0");
        QCOMPARE(calc.x(), 42.0);

        press(calc, "5 STO 1");
        press(calc, "RCL 0 RCL 1 +");
        QCOMPARE(calc.x(), 47.0);

        press(calc, "RCL 0 2 × STO 0");
        press(calc, "RCL 0");
        QCOMPARE(calc.x(), 84.0);

        press(calc, "1 0 STO + 0");
        press(calc, "RCL 0");
        QCOMPARE(calc.x(), 94.0);
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

        // Solve n: PV=-1000, PMT=200, i=10%, FV=0
        Backend calc2;
        press(calc2, "1 0 0 0 CHS PV 2 0 0 PMT 0 FV 1 0 i n");
        QVERIFY(calc2.x() > 0);
    }

    void tvmBeginMode() {
        Backend calc;
        press(calc, "g 7");   // BEG
        press(calc, "1 2 n 1 0 i 1 0 0 0 PV 0 FV PMT");
        double pmtBegin = calc.x();

        Backend calc2;
        press(calc2, "1 2 n 1 0 i 1 0 0 0 PV 0 FV PMT");
        double pmtEnd = calc2.x();

        QVERIFY(pmtBegin != pmtEnd);
        QVERIFY(std::abs(pmtBegin) < std::abs(pmtEnd));   // begin payments are smaller
    }

    void simpleInterest() {
        Backend calc;
        press(calc, "1 0 0 0 PV 6 n 1 i f i");   // f i = INT
        QVERIFY(calc.x() > 0);
    }

    void amortization() {
        Backend calc;
        press(calc, "3 6 0 n 0 . 5 i 1 0 0 0 0 0 PV 0 FV PMT");
        press(calc, "1 f PMT");   // f PMT = AMORT
        QVERIFY(calc.x() > 0);   // total interest
    }

    void cashFlowsNpvIrr() {
        Backend calc;
        press(calc, "1 0 0 0 0 CHS g PV");   // CFo = -10000
        press(calc, "5 0 0 0 g PMT");          // CF1 = 5000
        press(calc, "4 0 0 0 g PMT");          // CF2 = 4000
        press(calc, "3 0 0 0 g PMT");          // CF3 = 3000
        press(calc, "1 0 i f PV");             // f PV = NPV
        QVERIFY(calc.x() > 0);

        press(calc, "f FV");   // f FV = IRR
        QVERIFY(calc.x() > 0);
    }

    void depreciation() {
        Backend calc;
        press(calc, "1 0 0 0 0 PV 2 0 0 0 FV 5 n 1 f %T");   // SL year 1
        QCOMPARE(calc.x(), 1600.0);

        press(calc, "2 f Δ%");   // SOYD year 2
        QVERIFY(calc.x() > 0);

        press(calc, "3 f %");    // DB year 3
        QVERIFY(calc.x() > 0);
    }

    void dates() {
        Backend calc;
        press(calc, "g 4");   // D.MY
        press(calc, "2 8 . 1 2 2 0 2 5 ENTER 1 5 . 0 6 2 0 2 6 g EEX");   // ΔDYS
        QVERIFY(calc.x() > 150 && calc.x() < 180);

        press(calc, "R↓");              // bring the starting date back to Y
        press(calc, "1 5 0 g CHS");     // DATE: 28.12.2025 + 150 days
        QVERIFY(calc.display().contains(QStringLiteral(".")));
    }

    void bonds() {
        Backend calc;
        press(calc, "g 5");   // M.DY
        press(calc, "0 6 . 1 5 2 0 2 4 ENTER 1 2 . 1 5 2 0 2 5");
        press(calc, "ENTER 5 PMT 6 i");  // store coupon and yield
        press(calc, "R↓ R↓ x<>y");       // bring settlement/maturity to X/Y
        press(calc, "f y^x");            // f y^x = PRICE
        QVERIFY(calc.x() > 0);
        QVERIFY(calc.x() < 100.0);
    }

    void statistics() {
        Backend calc;
        // HP-12C Σ+ uses X as the x-value and Y as the y-value.
        press(calc, "1 ENTER 2 Σ+");
        press(calc, "2 ENTER 4 Σ+");
        press(calc, "3 ENTER 6 Σ+");

        press(calc, "g 0");   // g 0 = x̄ (mean of X values: 2, 4, 6)
        QCOMPARE(calc.x(), 4.0);

        press(calc, "2 g .");   // g . = s
        QVERIFY(calc.x() > 0);
    }

    void linearRegression() {
        Backend calc;
        press(calc, "1 ENTER 2 Σ+");
        press(calc, "2 ENTER 3 Σ+");
        press(calc, "3 ENTER 5 Σ+");

        press(calc, "5 g 2");   // g 2 = ŷ,r (predict y for x=5)
        QVERIFY(calc.x() > 0);

        press(calc, "4 g 1");   // g 1 = x̂,r (predict x for y=4)
        QVERIFY(calc.x() > 0);
    }

    void programExecution() {
        Backend calc;
        press(calc, "f R/S");   // P/R
        press(calc, "x^2");
        press(calc, "f R/S");   // P/R back to run
        press(calc, "5 R/S");
        QCOMPARE(calc.x(), 25.0);
    }

    void programGtoAndConditional() {
        Backend calc;
        press(calc, "f R/S");   // P/R
        press(calc, "1");
        press(calc, "g R↓");   // GTO
        press(calc, "0");
        press(calc, "4");      // GTO 04 (jump past the remaining code)
        press(calc, "2");
        press(calc, "f R/S");  // exit
        press(calc, "R/S");
        // Program: 1, GTO 04, 2. Should show 1 and never reach 2.
        QCOMPARE(calc.x(), 1.0);
    }

    void beginEndModes() {
        Backend calc;
        press(calc, "g 7");   // BEG
        QVERIFY(calc.beginMode());

        press(calc, "g 8");   // END
        QVERIFY(!calc.beginMode());
    }

    void dateModes() {
        Backend calc;
        press(calc, "g 4");   // D.MY
        QVERIFY(calc.dmyMode());

        press(calc, "g 5");   // M.DY
        QVERIFY(!calc.dmyMode());
    }

    void cleanupTestCase() {
    }

private:
    QTemporaryDir m_settingsDirectory;
};

QTEST_MAIN(Oma12cTest)
#include "tst_oma12c.moc"
