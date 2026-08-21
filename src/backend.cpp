#include "backend.h"

#include <QClipboard>
#include <QColor>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QHash>
#include <QLocale>
#include <QRect>
#include <QSettings>
#include <QTextStream>
#include <QVariantMap>
#include <cmath>

namespace {
const auto windowGeometrySetting = QStringLiteral("window/geometry");
const auto windowMaximizedSetting = QStringLiteral("window/maximized");

const int MAX_ENTRY_DIGITS = 10;

bool isNumberKey(const QString &key) {
    return key.size() == 1 && key.at(0).isDigit();
}

bool nearZero(double value) {
    return std::abs(value) < 1e-15;
}

double npvAtRate(double rate, const std::vector<double> &cashFlows,
                 const std::vector<int> &cashFlowCounts) {
    double npv = 0.0;
    int period = 0;
    for (std::size_t i = 0; i < cashFlows.size(); ++i) {
        for (int k = 0; k < cashFlowCounts[i]; ++k) {
            npv += cashFlows[i] * std::pow(1.0 + rate, -period);
            ++period;
        }
    }
    return npv;
}
}

Backend::Backend(QObject *parent) : QObject(parent) {
    loadDisplaySettings();
    loadOmarchyTheme();
    watchOmarchyTheme();

    connect(&m_themeWatcher, &QFileSystemWatcher::fileChanged, this, [this]() {
        loadOmarchyTheme();
        watchOmarchyTheme();
    });
    connect(&m_themeWatcher, &QFileSystemWatcher::directoryChanged, this, [this]() {
        loadOmarchyTheme();
        watchOmarchyTheme();
    });

    clearEntry();
}

QString Backend::display() const {
    if (m_error)
        return QStringLiteral("Error");

    if (m_prefix == Prefix::STO || m_prefix == Prefix::STOop)
        return QStringLiteral("STO");
    if (m_prefix == Prefix::RCL)
        return QStringLiteral("RCL");
    if (m_prefix == Prefix::GTO) {
        if (m_gtoFirstDigit >= 0)
            return QStringLiteral("GTO ") + QString::number(m_gtoFirstDigit);
        return QStringLiteral("GTO");
    }

    if (m_programMode) {
        if (m_programCounter < static_cast<int>(m_programSteps.size())) {
            const QString step = m_programSteps[m_programCounter];
            return QString::number(m_programCounter).rightJustified(2, QLatin1Char('0'))
                + QStringLiteral("- ") + programKeycode(step);
        }
        return QString::number(m_programCounter).rightJustified(2, QLatin1Char('0')) + QStringLiteral("-");
    }

    if (hasPendingEntry())
        return localizeNumber(m_entry);

    return formatForDisplay(currentX());
}

QString Backend::expression() const {
    if (m_error)
        return QStringLiteral("Error");

    switch (m_prefix) {
    case Prefix::f: return QStringLiteral("f");
    case Prefix::g: return QStringLiteral("g");
    default: break;
    }

    return QString();
}

QString Backend::prefix() const {
    switch (m_prefix) {
    case Prefix::f: return QStringLiteral("f");
    case Prefix::g: return QStringLiteral("g");
    default: return QString();
    }
}

QString Backend::recordableKey(const QString &key) const {
    const bool isF = (m_prefix == Prefix::f);
    const bool isG = (m_prefix == Prefix::g);
    const bool isSto = (m_prefix == Prefix::STO);
    const bool isRcl = (m_prefix == Prefix::RCL);
    const bool hasRclG = isRcl && m_secondaryPrefix == Prefix::g;
    const bool hasStoG = isSto && m_secondaryPrefix == Prefix::g;

    if (key == QStringLiteral("n")) {
        if (hasRclG) return QStringLiteral("RCL 12x");
        if (hasStoG) return QStringLiteral("STO 12x");
        if (isF) return QStringLiteral("CLEAR FIN");
        if (isG) return QStringLiteral("12x");
        return key;
    }
    if (key == QStringLiteral("i")) {
        if (hasRclG) return QStringLiteral("RCL 12÷");
        if (hasStoG) return QStringLiteral("STO 12÷");
        if (isF) return QStringLiteral("INT");
        if (isG) return QStringLiteral("12÷");
        return key;
    }
    if (key == QStringLiteral("PV")) {
        if (hasRclG) return QStringLiteral("RCL CFo");
        if (isF) return QStringLiteral("NPV");
        if (isG) return QStringLiteral("CFo");
        return key;
    }
    if (key == QStringLiteral("PMT")) {
        if (hasRclG) return QStringLiteral("RCL CFj");
        if (isF) return QStringLiteral("AMORT");
        if (isG) return QStringLiteral("CFj");
        return key;
    }
    if (key == QStringLiteral("FV")) {
        if (hasRclG) return QStringLiteral("RCL Nj");
        if (isF) return QStringLiteral("IRR");
        if (isG) return QStringLiteral("Nj");
        return key;
    }
    if (key == QStringLiteral("CHS")) {
        if (isG) return QStringLiteral("DATE");
        return key;
    }
    if (key == QStringLiteral("7")) {
        if (isG) return QStringLiteral("BEG");
        return key;
    }
    if (key == QStringLiteral("8")) {
        if (isG) return QStringLiteral("END");
        return key;
    }
    if (key == QStringLiteral("9")) {
        if (isG) return QStringLiteral("MEM");
        return key;
    }
    if (key == QStringLiteral("y^x")) {
        if (isF) return QStringLiteral("PRICE");
        if (isG) return QStringLiteral("√x");
        return key;
    }
    if (key == QStringLiteral("1/x")) {
        if (isF) return QStringLiteral("YTM");
        if (isG) return QStringLiteral("e^x");
        return key;
    }
    if (key == QStringLiteral("%T")) {
        if (isF) return QStringLiteral("SL");
        if (isG) return QStringLiteral("LN");
        return key;
    }
    if (key == QStringLiteral("Δ%")) {
        if (isF) return QStringLiteral("SOYD");
        if (isG) return QStringLiteral("FRAC");
        return key;
    }
    if (key == QStringLiteral("%")) {
        if (isF) return QStringLiteral("DB");
        if (isG) return QStringLiteral("INTG");
        return key;
    }
    if (key == QStringLiteral("EEX")) {
        if (isF) return QStringLiteral("FRAC");
        if (isG) return QStringLiteral("ΔDYS");
        return key;
    }
    if (key == QStringLiteral("R/S")) {
        if (isF) return QStringLiteral("P/R");
        if (isG) return QStringLiteral("PSE");
        return key;
    }
    if (key == QStringLiteral("SST")) {
        if (isF) return QStringLiteral("CLEAR Σ");
        if (isG) return QStringLiteral("BST");
        return key;
    }
    if (key == QStringLiteral("R↓")) {
        if (isF) return QStringLiteral("CLEAR PRGM");
        if (isG) return QStringLiteral("GTO");
        return key;
    }
    if (key == QStringLiteral("x<>y")) {
        if (isF) return QStringLiteral("CLEAR FIN");
        if (isG) return QStringLiteral("x≤y");
        return key;
    }
    if (key == QStringLiteral("CLx")) {
        if (isF) return QStringLiteral("CLEAR REG");
        if (isG) return QStringLiteral("x=0");
        return key;
    }
    if (key == QStringLiteral("ENTER")) {
        if (isF) return QStringLiteral("CLEAR PREFIX");
        if (isG) return QStringLiteral("LSTx");
        return key;
    }
    if (key == QStringLiteral("4")) {
        if (isG) return QStringLiteral("D.MY");
        return key;
    }
    if (key == QStringLiteral("5")) {
        if (isG) return QStringLiteral("M.DY");
        return key;
    }
    if (key == QStringLiteral("6")) {
        if (isG) return QStringLiteral("x↔w");
        return key;
    }
    if (key == QStringLiteral("1")) {
        if (isG) return QStringLiteral("x̂,r");
        return key;
    }
    if (key == QStringLiteral("2")) {
        if (isG) return QStringLiteral("ŷ,r");
        return key;
    }
    if (key == QStringLiteral("3")) {
        if (isG) return QStringLiteral("n!");
        return key;
    }
    if (key == QStringLiteral("0")) {
        if (isG) return QStringLiteral("x̄");
        return key;
    }
    if (key == QStringLiteral(".")) {
        if (isF) return QStringLiteral("SCI");
        if (isG) return QStringLiteral("s");
        return key;
    }
    if (key == QStringLiteral("Σ+")) {
        if (isF) return QStringLiteral("CLEAR Σ");
        if (isG) return QStringLiteral("Σ−");
        return key;
    }
    if (key == QStringLiteral("−")) {
        if (isG) return QStringLiteral("n!");
        return key;
    }
    if (key == QStringLiteral("÷")) {
        if (isG) return QStringLiteral("x≤y");
        return key;
    }
    if (key == QStringLiteral("×")) {
        if (isG) return QStringLiteral("x=0");
        return key;
    }

    if (isSto || isRcl) {
        if (key.startsWith(QStringLiteral("RCL ")) || key.startsWith(QStringLiteral("STO ")))
            return key;
        if (isSto) return QStringLiteral("STO ") + key;
        if (isRcl) return QStringLiteral("RCL ") + key;
    }

    return key;
}

void Backend::pressKey(const QString &key) {
    if (m_error && key != QStringLiteral("CLx") && key != QStringLiteral("ON")) {
        if (isNumberKey(key) || key == QStringLiteral(".")) {
            pressClear();
        } else {
            return;
        }
    }

    if (m_programMode) {
        if (key == QStringLiteral("ON")) {
            pressClear();
        } else if ((m_prefix == Prefix::RCL || m_prefix == Prefix::STO) &&
                   (key == QStringLiteral("f") || key == QStringLiteral("g"))) {
            m_secondaryPrefix = (key == QStringLiteral("f")) ? Prefix::f : Prefix::g;
        } else if (key == QStringLiteral("f") || key == QStringLiteral("g") ||
                   key == QStringLiteral("STO") || key == QStringLiteral("RCL") ||
                   key == QStringLiteral("GTO")) {
            if (key == QStringLiteral("f")) setPrefix(Prefix::f);
            else if (key == QStringLiteral("g")) setPrefix(Prefix::g);
            else if (key == QStringLiteral("STO")) setPrefix(Prefix::STO);
            else if (key == QStringLiteral("RCL")) setPrefix(Prefix::RCL);
            else if (key == QStringLiteral("GTO")) setPrefix(Prefix::GTO);
        } else if (key == QStringLiteral("SST")) {
            if (m_programCounter < static_cast<int>(m_programSteps.size()))
                ++m_programCounter;
        } else if (key == QStringLiteral("BST")) {
            if (m_programCounter > 0)
                --m_programCounter;
        } else if (m_prefix == Prefix::GTO) {
            pressGtoDigit(key);
        } else if (m_prefix != Prefix::None) {
            recordStep(recordableKey(key));
            m_prefix = Prefix::None;
            m_secondaryPrefix = Prefix::None;
        } else if (key == QStringLiteral("R/S")) {
            recordStep(QStringLiteral("R/S"));
        } else {
            recordStep(key);
        }
        emit stateChanged();
        return;
    }

    if (m_programRunning && (key == QStringLiteral("R/S") || key == QStringLiteral("PSE"))) {
        m_programRunning = false;
        emit stateChanged();
        return;
    }

    if (m_prefix != Prefix::None) {
        if (m_prefix == Prefix::GTO) {
            pressGtoDigit(key);
            emit stateChanged();
            return;
        }
        if ((m_prefix == Prefix::RCL || m_prefix == Prefix::STO) &&
            (key == QStringLiteral("f") || key == QStringLiteral("g"))) {
            m_secondaryPrefix = (key == QStringLiteral("f")) ? Prefix::f : Prefix::g;
            emit stateChanged();
            return;
        }
        executePrefixedKey(key);
        m_secondaryPrefix = Prefix::None;
        emit stateChanged();
        return;
    }

    if (isNumberKey(key)) {
        pressDigit(key);
    } else if (key == QStringLiteral(".")) {
        pressDecimal();
    } else if (key == QStringLiteral("ENTER")) {
        pressEnter();
    } else if (key == QStringLiteral("CHS")) {
        pressChs();
    } else if (key == QStringLiteral("EEX")) {
        pressEex();
    } else if (key == QStringLiteral("CLx")) {
        pressClx();
    } else if (key == QStringLiteral("ON")) {
        pressClear();
    } else if (key == QStringLiteral("f")) {
        setPrefix(Prefix::f);
    } else if (key == QStringLiteral("g")) {
        setPrefix(Prefix::g);
    } else if (key == QStringLiteral("STO")) {
        setPrefix(Prefix::STO);
    } else if (key == QStringLiteral("RCL")) {
        setPrefix(Prefix::RCL);
    } else if (key == QStringLiteral("GTO")) {
        setPrefix(Prefix::GTO);
    } else if (key == QStringLiteral("+")) {
        pressOperator(QStringLiteral("+"));
    } else if (key == QStringLiteral("−")) {
        pressOperator(QStringLiteral("−"));
    } else if (key == QStringLiteral("×")) {
        pressOperator(QStringLiteral("×"));
    } else if (key == QStringLiteral("÷")) {
        pressOperator(QStringLiteral("÷"));
    } else if (key == QStringLiteral("y^x")) {
        pressOperator(QStringLiteral("y^x"));
    } else if (key == QStringLiteral("R↓")) {
        pressStackOp(QStringLiteral("R↓"));
    } else if (key == QStringLiteral("x<>y")) {
        pressStackOp(QStringLiteral("x<>y"));
    } else if (key == QStringLiteral("R/S")) {
        executeProgram();
    } else if (key == QStringLiteral("SST")) {
        singleStep();
    } else if (key == QStringLiteral("Σ+")) {
        accumulateSigma(true);
    } else if (key == QStringLiteral("BST")) {
        if (m_programCounter > 0) --m_programCounter;
    } else if (key == QStringLiteral("PSE")) {
        // Pause is a no-op in run mode unless running a program.
    } else if (key == QStringLiteral("n") || key == QStringLiteral("i") ||
               key == QStringLiteral("PV") || key == QStringLiteral("PMT") ||
               key == QStringLiteral("FV")) {
        pressFinancialKey(key);
    } else if (key == QStringLiteral("%")) {
        percent();
    } else if (key == QStringLiteral("%T")) {
        percentTotal();
    } else if (key == QStringLiteral("Δ%")) {
        percentChange();
    } else if (key == QStringLiteral("1/x")) {
        pressOperator(QStringLiteral("1/x"));
    } else if (key == QStringLiteral("FIX")) {
        setPrefix(Prefix::FIX);
    } else if (key == QStringLiteral("SCI")) {
        m_displayMode = DisplayMode::SCI;
    } else if (key == QStringLiteral("ENG")) {
        m_displayMode = DisplayMode::ENG;
    } else if (key == QStringLiteral("D.MY")) {
        m_dmyMode = true;
    } else if (key == QStringLiteral("M.DY")) {
        m_dmyMode = false;
    } else if (key == QStringLiteral("BEG")) {
        m_beginMode = true;
    } else if (key == QStringLiteral("END")) {
        m_beginMode = false;
    } else if (key == QStringLiteral("P/R")) {
        m_programMode = !m_programMode;
    } else if (key == QStringLiteral("PSE")) {
        // no-op
    } else if (key == QStringLiteral("√x")) {
        pressOperator(QStringLiteral("√x"));
    } else if (key == QStringLiteral("x^2")) {
        pressOperator(QStringLiteral("x^2"));
    } else if (key == QStringLiteral("LN")) {
        pressOperator(QStringLiteral("LN"));
    } else if (key == QStringLiteral("e^x")) {
        pressOperator(QStringLiteral("e^x"));
    } else if (key == QStringLiteral("n!")) {
        pressOperator(QStringLiteral("n!"));
    } else if (key == QStringLiteral("INTG")) {
        pressOperator(QStringLiteral("INTG"));
    } else if (key == QStringLiteral("FRAC")) {
        pressOperator(QStringLiteral("FRAC"));
    } else if (key == QStringLiteral("12x")) {
        m_n = currentX() * 12; commitEntry(); m_entryReplace = true;
    } else if (key == QStringLiteral("12÷")) {
        m_i = currentX() / 12; commitEntry(); m_entryReplace = true;
    } else if (key == QStringLiteral("CFo")) {
        setCashFlowFirst();
    } else if (key == QStringLiteral("CFj")) {
        setCashFlowNext();
    } else if (key == QStringLiteral("Nj")) {
        setCashFlowCount();
    } else if (key == QStringLiteral("NPV")) {
        liftStack(); m_stack[0] = computeNpv(); m_entryReplace = true;
    } else if (key == QStringLiteral("IRR")) {
        liftStack(); m_stack[0] = computeIrr(); m_entryReplace = true;
    } else if (key == QStringLiteral("AMORT")) {
        amortize();
    } else if (key == QStringLiteral("PRICE")) {
        liftStack(); m_stack[0] = bondPrice(); m_entryReplace = true;
    } else if (key == QStringLiteral("YTM")) {
        liftStack(); m_stack[0] = bondYield(); m_entryReplace = true;
    } else if (key == QStringLiteral("SL")) {
        liftStack(); m_stack[0] = straightLineDepreciation(); m_entryReplace = true;
    } else if (key == QStringLiteral("SOYD")) {
        liftStack(); m_stack[0] = sumOfYearsDigitsDepreciation(); m_entryReplace = true;
    } else if (key == QStringLiteral("DB")) {
        liftStack(); m_stack[0] = decliningBalanceDepreciation(); m_entryReplace = true;
    } else if (key == QStringLiteral("DATE")) {
        computeFutureDate();
    } else if (key == QStringLiteral("ΔDYS")) {
        computeDaysBetween();
    } else if (key == QStringLiteral("x̄")) {
        computeMeanX();
    } else if (key == QStringLiteral("s")) {
        computeStdDevS();
    } else if (key == QStringLiteral("x̂,r")) {
        linearEstimateX();
    } else if (key == QStringLiteral("ŷ,r")) {
        linearEstimateY();
    } else if (key == QStringLiteral("x↔w")) {
        weightedAverage();
    } else if (key == QStringLiteral("LSTx")) {
        lastXToX();
    } else if (key == QStringLiteral("Σ−")) {
        accumulateSigma(false);
    } else if (key == QStringLiteral("x≤y")) {
        // Conditional in run mode is a no-op unless inside a running program.
    } else if (key == QStringLiteral("x=0")) {
        // Conditional in run mode is a no-op unless inside a running program.
    } else if (key == QStringLiteral("CLEAR FIN")) {
        clearFinancialRegisters();
        m_entryReplace = true;
    } else if (key == QStringLiteral("CLEAR REG")) {
        clearAllRegisters();
    } else if (key == QStringLiteral("CLEAR Σ")) {
        clearStatistics();
        m_entryReplace = true;
    } else if (key == QStringLiteral("CLEAR PRGM")) {
        clearProgram();
    } else if (key == QStringLiteral("CLEAR PREFIX")) {
        m_prefix = Prefix::None;
        m_error = false;
    } else if (key == QStringLiteral("INT")) {
        simpleInterest();
    } else if (key == QStringLiteral("RND")) {
        roundToDisplay();
    } else if (key == QStringLiteral("MEM")) {
        // placeholder
        m_entryReplace = true;
    } else if (key == QStringLiteral("RCL 12x")) {
        liftStack(); m_stack[0] = m_n / 12.0; m_entryReplace = true;
    } else if (key == QStringLiteral("RCL 12÷")) {
        liftStack(); m_stack[0] = m_i * 12.0; m_entryReplace = true;
    } else if (key == QStringLiteral("STO 12x")) {
        commitEntry(); m_n = currentX() * 12.0; m_entryReplace = true;
    } else if (key == QStringLiteral("STO 12÷")) {
        commitEntry(); m_i = currentX() / 12.0; m_entryReplace = true;
    } else if (key == QStringLiteral("RCL CFo")) {
        liftStack();
        m_stack[0] = m_cashFlows.empty() ? 0 : m_cashFlows[0];
        m_entryReplace = true;
    } else if (key == QStringLiteral("RCL CFj")) {
        commitEntry();
        const int idx = static_cast<int>(m_stack[0]);
        liftStack();
        if (idx >= 0 && idx < static_cast<int>(m_cashFlows.size()))
            m_stack[0] = m_cashFlows[idx];
        else
            m_error = true;
        m_entryReplace = true;
    } else if (key == QStringLiteral("RCL Nj")) {
        commitEntry();
        const int idx = static_cast<int>(m_stack[0]);
        liftStack();
        if (idx >= 0 && idx < static_cast<int>(m_cashFlowCounts.size()))
            m_stack[0] = m_cashFlowCounts[idx];
        else
            m_error = true;
        m_entryReplace = true;
    } else if (key.startsWith(QStringLiteral("RCL "))) {
        const QString sub = key.mid(4);
        pressRecallKey(sub);
    } else if (key.startsWith(QStringLiteral("STO "))) {
        const QString sub = key.mid(4);
        pressStorageKey(sub);
    } else if (key == QStringLiteral("GTO")) {
        setPrefix(Prefix::GTO);
    }

    emit stateChanged();
}

void Backend::pressDigit(const QString &digit) {
    if (m_entryReplace) {
        clearEntry();
        m_entryReplace = false;
    }

    if (m_entry.isEmpty() && digit == QStringLiteral("0")) {
        m_entry = digit;
        return;
    }

    if (m_entry == QStringLiteral("0")) {
        m_entry = digit;
        return;
    } else if (m_entry == QStringLiteral("-0")) {
        m_entry = QStringLiteral("-") + digit;
        return;
    }

    int digits = 0;
    for (const QChar c : m_entry) {
        if (c.isDigit())
            ++digits;
    }

    if (m_entryHasExponent) {
        const int expPos = m_entry.indexOf(QLatin1Char('e'));
        if (expPos >= 0 && m_entry.size() - expPos > 2)
            return;
    } else if (digits >= MAX_ENTRY_DIGITS) {
        return;
    }

    m_entry += digit;
}

void Backend::pressDecimal() {
    if (m_entryReplace) {
        clearEntry();
        m_entry = QStringLiteral("0");
        m_entryReplace = false;
    }

    if (m_entryHasExponent)
        return;

    if (!m_entryHasDecimal) {
        m_entryHasDecimal = true;
        if (m_entry.isEmpty() || m_entry == QStringLiteral("-"))
            m_entry += QStringLiteral("0.");
        else
            m_entry += QLatin1Char('.');
    }
}

void Backend::pressEnter() {
    if (hasPendingEntry()) {
        commitEntry();
    } else {
        liftStack();
    }
}

void Backend::pressChs() {
    if (m_entry.isEmpty() && m_entryReplace) {
        m_stack[0] = -m_stack[0];
    } else if (m_entryHasExponent && m_entry.contains(QLatin1Char('e'))) {
        const int expPos = m_entry.indexOf(QLatin1Char('e'));
        if (m_entry.size() > expPos + 1 && m_entry.at(expPos + 1) == QLatin1Char('-')) {
            m_entry.remove(expPos + 1, 1);
        } else {
            m_entry.insert(expPos + 1, QLatin1Char('-'));
        }
    } else {
        if (m_entry.startsWith(QLatin1Char('-'))) {
            m_entry.remove(0, 1);
            m_entryIsNegative = false;
        } else {
            m_entry.prepend(QLatin1Char('-'));
            m_entryIsNegative = true;
        }
    }
}

void Backend::pressEex() {
    if (m_entryReplace) {
        clearEntry();
        m_entry = QStringLiteral("1");
        m_entryReplace = false;
    }

    if (!m_entryHasExponent) {
        m_entryHasExponent = true;
        m_entry += QLatin1Char('e');
    }
}

void Backend::pressClear() {
    m_stack = {0, 0, 0, 0};
    m_lastX = 0;
    clearFinancialRegisters();
    clearStatistics();
    clearProgram();
    clearEntry();
    m_prefix = Prefix::None;
    m_error = false;
    m_displayMode = DisplayMode::FIX;
    m_displayDigits = 2;
    m_beginMode = false;
    m_dmyMode = false;
    m_programMode = false;
}

void Backend::pressClx() {
    if (m_prefix != Prefix::None) {
        m_prefix = Prefix::None;
        return;
    }
    clearEntry();
    m_stack[0] = 0;
    m_error = false;
}

void Backend::pressOperator(const QString &op) {
    const double x = currentX();
    commitEntry();

    const double y = m_stack[1];
    double result = 0;

    if (op == QStringLiteral("+"))
        result = y + x;
    else if (op == QStringLiteral("−"))
        result = y - x;
    else if (op == QStringLiteral("×"))
        result = y * x;
    else if (op == QStringLiteral("÷")) {
        if (nearZero(x)) { m_error = true; return; }
        result = y / x;
    } else if (op == QStringLiteral("y^x")) {
        if (y < 0 && std::floor(x) != x) { m_error = true; return; }
        result = std::pow(y, x);
    } else if (op == QStringLiteral("1/x")) {
        if (nearZero(x)) { m_error = true; return; }
        result = 1.0 / x;
    } else if (op == QStringLiteral("√x")) {
        if (x < 0) { m_error = true; return; }
        result = std::sqrt(x);
    } else if (op == QStringLiteral("x^2")) {
        result = x * x;
    } else if (op == QStringLiteral("LN")) {
        if (x <= 0) { m_error = true; return; }
        result = std::log(x);
    } else if (op == QStringLiteral("e^x")) {
        result = std::exp(x);
    } else if (op == QStringLiteral("n!")) {
        if (x < 0 || std::floor(x) != x) { m_error = true; return; }
        result = std::tgamma(x + 1);
    } else if (op == QStringLiteral("INTG")) {
        result = std::floor(x);
    } else if (op == QStringLiteral("FRAC")) {
        result = x - std::floor(x);
    }

    if (!std::isfinite(result)) { m_error = true; return; }

    m_lastX = x;
    dropStack();
    m_stack[0] = result;
    m_entryReplace = true;
}

void Backend::pressStackOp(const QString &op) {
    commitEntry();
    if (op == QStringLiteral("R↓"))
        rollDown();
    else if (op == QStringLiteral("x<>y"))
        swapXY();
}

void Backend::pressFinancialKey(const QString &key) {
    if (hasPendingEntry()) {
        const double value = currentX();
        commitEntry();
        if (key == QStringLiteral("n")) m_n = value;
        else if (key == QStringLiteral("i")) m_i = value;
        else if (key == QStringLiteral("PV")) m_pv = value;
        else if (key == QStringLiteral("PMT")) m_pmt = value;
        else if (key == QStringLiteral("FV")) m_fv = value;
        m_entryReplace = true;
    } else {
        const double result = solveTvm(key);
        if (m_error)
            return;
        liftStack();
        m_stack[0] = result;
        if (key == QStringLiteral("n")) m_n = result;
        else if (key == QStringLiteral("i")) m_i = result;
        else if (key == QStringLiteral("PV")) m_pv = result;
        else if (key == QStringLiteral("PMT")) m_pmt = result;
        else if (key == QStringLiteral("FV")) m_fv = result;
        m_entryReplace = true;
    }
}

void Backend::pressStorageKey(const QString &key) {
    if (m_prefix == Prefix::STO) {
        if (isNumberKey(key)) {
            const int reg = key.toInt();
            m_storageRegisters[reg] = currentX();
            commitEntry();
            m_entryReplace = true;
            m_prefix = Prefix::None;
        } else if (key == QStringLiteral("+") || key == QStringLiteral("−") ||
                   key == QStringLiteral("×") || key == QStringLiteral("÷")) {
            m_prefix = Prefix::STOop;
            m_stoOp = key;
        } else if (key == QStringLiteral("n")) { m_n = currentX(); commitEntry(); m_prefix = Prefix::None; m_entryReplace = true; }
        else if (key == QStringLiteral("i")) { m_i = currentX(); commitEntry(); m_prefix = Prefix::None; m_entryReplace = true; }
        else if (key == QStringLiteral("PV")) { m_pv = currentX(); commitEntry(); m_prefix = Prefix::None; m_entryReplace = true; }
        else if (key == QStringLiteral("PMT")) { m_pmt = currentX(); commitEntry(); m_prefix = Prefix::None; m_entryReplace = true; }
        else if (key == QStringLiteral("FV")) { m_fv = currentX(); commitEntry(); m_prefix = Prefix::None; m_entryReplace = true; }
    } else if (m_prefix == Prefix::STOop) {
        if (isNumberKey(key)) {
            const int reg = key.toInt();
            const double current = m_storageRegisters[reg];
            const double x = currentX();
            double result = 0;
            if (m_stoOp == QStringLiteral("+")) result = current + x;
            else if (m_stoOp == QStringLiteral("−")) result = current - x;
            else if (m_stoOp == QStringLiteral("×")) result = current * x;
            else if (m_stoOp == QStringLiteral("÷")) {
                if (nearZero(x)) { m_error = true; return; }
                result = current / x;
            }
            m_storageRegisters[reg] = result;
            commitEntry();
            m_entryReplace = true;
            m_prefix = Prefix::None;
        }
    }
}

void Backend::pressRecallKey(const QString &key) {
    if (isNumberKey(key)) {
        const int reg = key.toInt();
        const auto it = m_storageRegisters.find(reg);
        const double value = (it != m_storageRegisters.end()) ? it->second : 0;
        liftStack();
        m_stack[0] = value;
        m_entryReplace = true;
        m_prefix = Prefix::None;
    } else if (key == QStringLiteral("n")) { liftStack(); m_stack[0] = m_n; }
    else if (key == QStringLiteral("i")) { liftStack(); m_stack[0] = m_i; }
    else if (key == QStringLiteral("PV")) { liftStack(); m_stack[0] = m_pv; }
    else if (key == QStringLiteral("PMT")) { liftStack(); m_stack[0] = m_pmt; }
    else if (key == QStringLiteral("FV")) { liftStack(); m_stack[0] = m_fv; }
    m_prefix = Prefix::None;
    m_entryReplace = true;
}

void Backend::executePrefixedKey(const QString &key) {
    // RCL/STO followed by a function key (f or g) is a combined command.
    if (m_secondaryPrefix != Prefix::None &&
        (m_prefix == Prefix::RCL || m_prefix == Prefix::STO)) {
        const QString action = recordableKey(key);
        m_prefix = Prefix::None;
        m_secondaryPrefix = Prefix::None;
        pressKey(action);
        return;
    }

    if (m_prefix == Prefix::STO || m_prefix == Prefix::STOop) {
        pressStorageKey(key);
        return;
    }
    if (m_prefix == Prefix::RCL) {
        pressRecallKey(key);
        return;
    }
    if (m_prefix == Prefix::GTO) {
        if (isNumberKey(key)) {
            int step = key.toInt();
            if (step >= 0 && step <= 99)
                goToStep(step);
        }
        m_prefix = Prefix::None;
        return;
    }
    if (m_prefix == Prefix::FIX) {
        if (isNumberKey(key)) {
            m_displayDigits = key.toInt();
            m_displayMode = DisplayMode::FIX;
        } else if (key == QStringLiteral(".")) {
            m_displayMode = DisplayMode::SCI;
        }
        m_prefix = Prefix::None;
        return;
    }

    const bool isF = (m_prefix == Prefix::f);
    m_prefix = Prefix::None;

    if (key == QStringLiteral("n")) {
        if (isF) clearFinancialRegisters();
        else { m_n = currentX() * 12; commitEntry(); m_entryReplace = true; }
    } else if (key == QStringLiteral("i")) {
        if (isF) simpleInterest();
        else { m_i = currentX() / 12; commitEntry(); m_entryReplace = true; }
    } else if (key == QStringLiteral("PV")) {
        if (isF) { liftStack(); m_stack[0] = computeNpv(); m_entryReplace = true; }
        else setCashFlowFirst();
    } else if (key == QStringLiteral("PMT")) {
        if (isF) amortize();
        else setCashFlowNext();
    } else if (key == QStringLiteral("FV")) {
        if (isF) { liftStack(); m_stack[0] = computeIrr(); m_entryReplace = true; }
        else setCashFlowCount();
    } else if (key == QStringLiteral("y^x")) {
        if (isF) { liftStack(); m_stack[0] = bondPrice(); m_entryReplace = true; }
        else pressOperator(QStringLiteral("√x"));
    } else if (key == QStringLiteral("1/x")) {
        if (isF) { liftStack(); m_stack[0] = bondYield(); m_entryReplace = true; }
        else pressOperator(QStringLiteral("e^x"));
    } else if (key == QStringLiteral("%T")) {
        if (isF) { liftStack(); m_stack[0] = straightLineDepreciation(); m_entryReplace = true; }
        else pressOperator(QStringLiteral("LN"));
    } else if (key == QStringLiteral("Δ%")) {
        if (isF) { liftStack(); m_stack[0] = sumOfYearsDigitsDepreciation(); m_entryReplace = true; }
        else pressOperator(QStringLiteral("FRAC"));
    } else if (key == QStringLiteral("%")) {
        if (isF) { liftStack(); m_stack[0] = decliningBalanceDepreciation(); m_entryReplace = true; }
        else pressOperator(QStringLiteral("INTG"));
    } else if (key == QStringLiteral("R/S")) {
        if (isF) m_programMode = !m_programMode;
        else { /* PSE placeholder in execute; handled by pressKey for PSE */ }
    } else if (key == QStringLiteral("SST")) {
        if (isF) clearStatistics();
        else { /* BST placeholder in execute; handled by pressKey for BST */ }
    } else if (key == QStringLiteral("R↓")) {
        if (isF) clearProgram();
        else setPrefix(Prefix::GTO);
    } else if (key == QStringLiteral("x<>y")) {
        if (isF) clearFinancialRegisters();
        else { /* x<=y is handled in program execution */ }
    } else if (key == QStringLiteral("CLx")) {
        if (isF) clearAllRegisters();
        else { /* x=0 is handled in program execution */ }
    } else if (key == QStringLiteral("ENTER")) {
        if (isF) {
            m_prefix = Prefix::None;
            m_error = false;
        } else {
            lastXToX();
        }
    } else if (key == QStringLiteral("CHS")) {
        if (isF) { }
        else computeFutureDate();
    } else if (key == QStringLiteral("EEX")) {
        if (isF) pressOperator(QStringLiteral("FRAC"));
        else computeDaysBetween();
    } else if (isNumberKey(key)) {
        if (isF) {
            m_displayDigits = key.toInt();
            m_displayMode = DisplayMode::FIX;
        } else {
            if (key == QStringLiteral("0")) {
                computeMeanX();
            } else if (key == QStringLiteral("1")) {
                linearEstimateX();
            } else if (key == QStringLiteral("2")) {
                linearEstimateY();
            } else if (key == QStringLiteral("3")) {
                pressOperator(QStringLiteral("n!"));
            } else if (key == QStringLiteral("4")) {
                m_dmyMode = true;
                m_entryReplace = true;
            } else if (key == QStringLiteral("5")) {
                m_dmyMode = false;
                m_entryReplace = true;
            } else if (key == QStringLiteral("6")) {
                weightedAverage();
            } else if (key == QStringLiteral("7")) {
                m_beginMode = true;
                m_entryReplace = true;
            } else if (key == QStringLiteral("8")) {
                m_beginMode = false;
                m_entryReplace = true;
            } else if (key == QStringLiteral("9")) {
                m_entryReplace = true;
            } else {
                pressRecallKey(key);
            }
        }
    } else if (key == QStringLiteral(".")) {
        if (isF) m_displayMode = DisplayMode::SCI;
        else computeStdDevS();
    } else if (key == QStringLiteral("Σ+")) {
        if (isF) clearStatistics();
        else accumulateSigma(false);
    } else if (key == QStringLiteral("−")) {
        if (!isF) pressOperator(QStringLiteral("n!"));
    } else if (key == QStringLiteral("÷")) {
        if (!isF) { /* x<=y conditional is a program step */ }
    } else if (key == QStringLiteral("×")) {
        if (!isF) { /* x=0 conditional is a program step */ }
    }
}

void Backend::setPrefix(Prefix prefix) {
    m_prefix = prefix;
}

double Backend::currentX() const {
    if (hasPendingEntry())
        return parseEntry();
    return m_stack[0];
}

double Backend::parseEntry() const {
    if (m_entry.isEmpty())
        return m_stack[0];

    bool ok = false;
    double value = toDouble(m_entry, &ok);
    if (!ok || !std::isfinite(value))
        return 0;
    return value;
}

bool Backend::hasPendingEntry() const {
    return !m_entry.isEmpty();
}

void Backend::commitEntry() {
    if (m_entry.isEmpty())
        return;

    bool ok = false;
    double value = toDouble(m_entry, &ok);
    if (!ok || !std::isfinite(value)) {
        m_error = true;
        return;
    }

    liftStack();
    m_stack[0] = value;
    clearEntry();
}

void Backend::clearEntry() {
    m_entry.clear();
    m_entryHasDecimal = false;
    m_entryHasExponent = false;
    m_entryExponentHasSign = false;
    m_entryIsNegative = false;
    m_entryReplace = true;
}

void Backend::liftStack() {
    m_stack[3] = m_stack[2];
    m_stack[2] = m_stack[1];
    m_stack[1] = m_stack[0];
}

void Backend::dropStack() {
    m_stack[0] = m_stack[1];
    m_stack[1] = m_stack[2];
    m_stack[2] = m_stack[3];
}

void Backend::rollDown() {
    const double x = m_stack[0];
    m_stack[0] = m_stack[1];
    m_stack[1] = m_stack[2];
    m_stack[2] = m_stack[3];
    m_stack[3] = x;
}

void Backend::swapXY() {
    std::swap(m_stack[0], m_stack[1]);
}

void Backend::lastXToX() {
    liftStack();
    m_stack[0] = m_lastX;
}

void Backend::percent() {
    const double x = currentX();
    commitEntry();
    const double y = m_stack[1];
    const double result = y * x / 100.0;
    liftStack();
    m_stack[0] = result;
    m_lastX = x;
    m_entryReplace = true;
}

void Backend::percentChange() {
    const double x = currentX();
    commitEntry();
    const double y = m_stack[1];
    if (nearZero(y)) { m_error = true; return; }
    const double result = (x - y) / y * 100.0;
    m_lastX = x;
    dropStack();
    m_stack[0] = result;
    m_entryReplace = true;
}

void Backend::percentTotal() {
    const double x = currentX();
    commitEntry();
    const double y = m_stack[1];
    if (nearZero(y)) { m_error = true; return; }
    const double result = x / y * 100.0;
    m_lastX = x;
    dropStack();
    m_stack[0] = result;
    m_entryReplace = true;
}

void Backend::simpleInterest() {
    const double principal = m_stack[1];
    const double rate = currentX() / 100.0;
    const double interest = principal * rate * (m_n / 12.0);
    liftStack();
    m_stack[0] = interest;
    m_stack[1] = principal + interest;
    m_entryReplace = true;
}

void Backend::roundToDisplay() {
    const double value = currentX();
    commitEntry();
    m_stack[0] = toDouble(formatForDisplay(value));
    m_entryReplace = true;
}

void Backend::clearAllRegisters() {
    clearFinancialRegisters();
    clearStatistics();
    m_stack = {0, 0, 0, 0};
    m_lastX = 0;
    clearEntry();
    m_error = false;
    m_prefix = Prefix::None;
    m_stoOp.clear();
}

void Backend::clearFinancialRegisters() {
    m_n = m_i = m_pv = m_pmt = m_fv = 0;
}

void Backend::clearStatistics() {
    for (int i = 1; i <= 6; ++i)
        m_storageRegisters[i] = 0;
}

void Backend::accumulateSigma(bool add) {
    commitEntry();
    const double x = m_stack[0];
    const double y = m_stack[1];
    const double sign = add ? 1.0 : -1.0;

    m_storageRegisters[1] += sign;
    m_storageRegisters[2] += sign * x;
    m_storageRegisters[3] += sign * x * x;
    m_storageRegisters[4] += sign * y;
    m_storageRegisters[5] += sign * y * y;
    m_storageRegisters[6] += sign * x * y;

    m_stack[0] = m_storageRegisters[1];
    m_entryReplace = true;
}

void Backend::computeMeanX() {
    commitEntry();
    const double n = m_storageRegisters[1];
    if (nearZero(n)) { m_error = true; return; }
    liftStack();
    m_stack[0] = m_storageRegisters[2] / n;
    m_entryReplace = true;
}

void Backend::computeStdDevS() {
    commitEntry();
    const double n = m_storageRegisters[1];
    if (n < 2) { m_error = true; return; }
    const double meanX = m_storageRegisters[2] / n;
    const double meanY = m_storageRegisters[4] / n;
    const double sX = std::sqrt((m_storageRegisters[3] - n * meanX * meanX) / (n - 1));
    const double sY = std::sqrt((m_storageRegisters[5] - n * meanY * meanY) / (n - 1));
    liftStack();
    m_stack[0] = sY;
    m_stack[1] = sX;
    m_entryReplace = true;
}

void Backend::linearEstimateX() {
    commitEntry();
    const double n = m_storageRegisters[1];
    if (nearZero(n)) { m_error = true; return; }
    const double sumX = m_storageRegisters[2];
    const double sumY = m_storageRegisters[4];
    const double sumX2 = m_storageRegisters[3];
    const double sumXY = m_storageRegisters[6];
    const double meanX = sumX / n;
    const double meanY = sumY / n;
    const double sxx = sumX2 - n * meanX * meanX;
    const double sxy = sumXY - n * meanX * meanY;
    if (nearZero(sxx)) { m_error = true; return; }
    const double slope = sxy / sxx;
    const double intercept = meanY - slope * meanX;
    const double y = currentX();
    const double result = (y - intercept) / slope;
    liftStack();
    m_stack[0] = result;
    m_entryReplace = true;
}

void Backend::linearEstimateY() {
    commitEntry();
    const double n = m_storageRegisters[1];
    if (nearZero(n)) { m_error = true; return; }
    const double sumX = m_storageRegisters[2];
    const double sumY = m_storageRegisters[4];
    const double sumX2 = m_storageRegisters[3];
    const double sumXY = m_storageRegisters[6];
    const double meanX = sumX / n;
    const double meanY = sumY / n;
    const double sxx = sumX2 - n * meanX * meanX;
    const double sxy = sumXY - n * meanX * meanY;
    if (nearZero(sxx)) { m_error = true; return; }
    const double slope = sxy / sxx;
    const double intercept = meanY - slope * meanX;
    const double x = currentX();
    const double result = intercept + slope * x;
    liftStack();
    m_stack[0] = result;
    m_entryReplace = true;
}

void Backend::weightedAverage() {
    commitEntry();
    m_entryReplace = true;
}

// TVM helpers.
double Backend::tvmFactor() const {
    const double rate = m_i / 100.0;
    if (nearZero(rate))
        return 1.0;
    return std::pow(1.0 + rate, m_n);
}

double Backend::calculateFv() const {
    const double rate = m_i / 100.0;
    const double factor = tvmFactor();
    const double annuity = nearZero(rate) ? m_n : (factor - 1.0) / rate;
    const double begin = m_beginMode ? 1.0 + rate : 1.0;
    return -(m_pv * factor + m_pmt * annuity * begin);
}

double Backend::calculatePv() const {
    const double rate = m_i / 100.0;
    const double factor = tvmFactor();
    const double annuity = nearZero(rate) ? m_n : (factor - 1.0) / rate;
    const double begin = m_beginMode ? 1.0 + rate : 1.0;
    return -(m_fv + m_pmt * annuity * begin) / factor;
}

double Backend::calculatePmt() const {
    const double rate = m_i / 100.0;
    if (nearZero(rate)) {
        return -(m_pv + m_fv) / m_n;
    }
    const double factor = tvmFactor();
    const double annuity = (factor - 1.0) / rate;
    const double begin = m_beginMode ? 1.0 + rate : 1.0;
    return -(m_pv * factor + m_fv) / (annuity * begin);
}

double Backend::calculateN() const {
    const double rate = m_i / 100.0;
    if (nearZero(rate)) {
        if (nearZero(m_pmt)) { return 0; }
        return -(m_pv + m_fv) / m_pmt;
    }
    const double pmtBegin = m_beginMode ? m_pmt * (1.0 + rate) : m_pmt;
    const double numerator = pmtBegin - m_fv;
    const double denominator = pmtBegin + m_pv * rate;
    if (nearZero(denominator) || numerator / denominator <= 0)
        return 0;
    return std::log(numerator / denominator) / std::log(1.0 + rate);
}

namespace {
double tvmResidual(double rate, double n, double pv, double pmt, double fv, bool begin) {
    if (nearZero(rate))
        return pv + pmt * n + fv;
    const double f = std::pow(1.0 + rate, n);
    const double a = (f - 1.0) / rate;
    const double b = begin ? 1.0 + rate : 1.0;
    return pv * f + pmt * a * b + fv;
}

double tvmDerivative(double rate, double n, double pv, double pmt, double fv, bool begin) {
    (void)fv;
    if (nearZero(rate))
        return pmt * n * (n - 1.0) / 2.0;
    const double f = std::pow(1.0 + rate, n);
    const double a = (f - 1.0) / rate;
    const double b = begin ? 1.0 + rate : 1.0;
    const double dF = n * f / (1.0 + rate);
    const double dA = (dF * rate - (f - 1.0)) / (rate * rate);
    const double dB = begin ? 1.0 : 0.0;
    return pv * dF + pmt * (dA * b + a * dB);
}
}

double Backend::calculateI() const {
    if (nearZero(m_n)) { return 0; }

    const double decimalRate = m_i / 100.0;
    double rate = decimalRate != 0 ? decimalRate : 0.1;
    for (int iter = 0; iter < 100; ++iter) {
        const double f = tvmResidual(rate, m_n, m_pv, m_pmt, m_fv, m_beginMode);
        const double df = tvmDerivative(rate, m_n, m_pv, m_pmt, m_fv, m_beginMode);
        if (nearZero(df)) break;
        const double delta = f / df;
        rate -= delta;
        if (std::abs(delta) < 1e-12) break;
    }
    return rate * 100.0;
}

double Backend::solveTvm(const QString &variable) const {
    if (variable == QStringLiteral("n")) return calculateN();
    if (variable == QStringLiteral("i")) return calculateI();
    if (variable == QStringLiteral("PV")) return calculatePv();
    if (variable == QStringLiteral("PMT")) return calculatePmt();
    if (variable == QStringLiteral("FV")) return calculateFv();
    return 0;
}

void Backend::amortize() {
    commitEntry();
    const int periods = static_cast<int>(m_stack[0]);
    if (periods < 1) { m_error = true; return; }
    const double rate = m_i / 100.0;
    double balance = m_pv;
    double totalInterest = 0;
    double totalPrincipal = 0;
    for (int k = 0; k < periods; ++k) {
        const double interest = balance * rate;
        const double principal = m_pmt - interest;
        balance += principal;
        totalInterest += interest;
        totalPrincipal += principal;
    }
    m_pv = balance;
    m_n -= periods;
    if (m_n < 0) m_n = 0;
    liftStack();
    m_stack[0] = totalInterest;
    m_stack[1] = totalPrincipal;
    m_entryReplace = true;
}

// Cash flow analysis.
void Backend::setCashFlowFirst() {
    commitEntry();
    m_cashFlows.clear();
    m_cashFlowCounts.clear();
    m_cashFlows.push_back(m_stack[0]);
    m_cashFlowCounts.push_back(1);
    m_cashFlowIndex = 0;
    m_entryReplace = true;
}

void Backend::setCashFlowNext() {
    commitEntry();
    m_cashFlows.push_back(m_stack[0]);
    m_cashFlowCounts.push_back(1);
    m_cashFlowIndex = static_cast<int>(m_cashFlows.size()) - 1;
    m_entryReplace = true;
}

void Backend::setCashFlowCount() {
    commitEntry();
    const int count = static_cast<int>(m_stack[0]);
    if (count < 1 || m_cashFlowIndex < 0 || m_cashFlowIndex >= static_cast<int>(m_cashFlowCounts.size())) {
        m_error = true;
        return;
    }
    m_cashFlowCounts[m_cashFlowIndex] = count;
    m_entryReplace = true;
}

double Backend::computeNpv() const {
    const double rate = m_i / 100.0;
    if (m_cashFlows.empty()) return 0.0;
    return npvAtRate(rate, m_cashFlows, m_cashFlowCounts);
}

double Backend::computeIrr() const {
    if (m_cashFlows.empty()) return 0.0;

    double rate = m_i / 100.0;
    if (nearZero(rate))
        rate = 0.1;

    for (int iter = 0; iter < 100; ++iter) {
        const double f = npvAtRate(rate, m_cashFlows, m_cashFlowCounts);
        const double fPlus = npvAtRate(rate + 1e-8, m_cashFlows, m_cashFlowCounts);
        const double df = (fPlus - f) / 1e-8;
        if (nearZero(df)) break;
        const double delta = f / df;
        rate -= delta;
        if (std::abs(delta) < 1e-12) break;
    }
    return rate * 100.0;
}

// Depreciation.
double Backend::straightLineDepreciation() const {
    const double cost = m_pv;
    const double salvage = m_fv;
    const double life = m_n;
    if (nearZero(life)) return 0;
    return (cost - salvage) / life;
}

double Backend::sumOfYearsDigitsDepreciation() const {
    const double cost = m_pv;
    const double salvage = m_fv;
    const double life = m_n;
    const double year = currentX();
    if (nearZero(life) || year < 1 || year > life) return 0;
    const double sumYears = life * (life + 1) / 2.0;
    return (cost - salvage) * (life - year + 1) / sumYears;
}

double Backend::decliningBalanceDepreciation() const {
    const double cost = m_pv;
    const double salvage = m_fv;
    const double life = m_n;
    const double year = currentX();
    if (nearZero(life) || year < 1) return 0;
    const double rate = 2.0 / life;
    const double prevValue = cost * std::pow(1.0 - rate, year - 1);
    const double value = cost * std::pow(1.0 - rate, year);
    const double dep = prevValue - value;
    if (value < salvage) return prevValue - salvage;
    return dep;
}

// Date arithmetic.
QDate Backend::parseDate(double value) const {
    double absValue = std::abs(value);
    const QString text = QString::number(absValue, 'f', 6);
    const QChar dot = QLatin1Char('.');
    const QString integerPart = text.section(dot, 0, 0);
    const QString fractionalPart = text.section(dot, 1, 1);

    int first = integerPart.toInt();
    int second = 0;
    int year = 1900;

    if (!fractionalPart.isEmpty()) {
        const int padded = fractionalPart.rightJustified(6, QLatin1Char('0')).toInt();
        second = padded / 10000;
        const int yearDigits = padded % 10000;
        year = yearDigits;
        if (year < 100)
            year += 1900;
    }

    int day, month;
    if (m_dmyMode) {
        day = first;
        month = second;
    } else {
        month = first;
        day = second;
    }

    return QDate(year, month, day);
}

double Backend::encodeDate(const QDate &date) const {
    int first, second;
    if (m_dmyMode) {
        first = date.day();
        second = date.month() * 10000 + date.year();
    } else {
        first = date.month();
        second = date.day() * 10000 + date.year();
    }
    return first + second / 1000000.0;
}

void Backend::computeDaysBetween() {
    commitEntry();
    const QDate later = parseDate(m_stack[0]);
    const QDate earlier = parseDate(m_stack[1]);
    if (!later.isValid() || !earlier.isValid()) { m_error = true; return; }
    liftStack();
    m_stack[0] = earlier.daysTo(later);
    m_entryReplace = true;
}

void Backend::computeFutureDate() {
    commitEntry();
    const int days = static_cast<int>(m_stack[0]);
    const QDate start = parseDate(m_stack[1]);
    if (!start.isValid()) { m_error = true; return; }
    const QDate result = start.addDays(days);
    liftStack();
    m_stack[0] = encodeDate(result);
    m_entryReplace = true;
}

// Bonds (simplified).
namespace {
// 30/360 US day count (used by HP-12C for bonds).
int days360(const QDate &start, const QDate &end) {
    int d1 = start.day();
    int m1 = start.month();
    int y1 = start.year();
    int d2 = end.day();
    int m2 = end.month();
    int y2 = end.year();

    if (d1 == 31) d1 = 30;
    if (d2 == 31 && d1 == 30) d2 = 30;

    return 360 * (y2 - y1) + 30 * (m2 - m1) + (d2 - d1);
}

QDate addMonths360(const QDate &date, int months) {
    int y = date.year();
    int m = date.month() + months;
    y += (m - 1) / 12;
    m = ((m - 1) % 12) + 1;
    int d = date.day();
    if (d > 28) {
        QDate candidate(y, m, 1);
        d = std::min(d, candidate.daysInMonth());
    }
    return QDate(y, m, d);
}

QDate nextCouponDate(const QDate &settlement, const QDate &maturity, bool &ok) {
    ok = false;
    if (!settlement.isValid() || !maturity.isValid())
        return QDate();

    // Maturity is always a coupon date; work backwards in 6-month steps.
    QDate coupon = maturity;
    while (coupon > settlement) {
        coupon = addMonths360(coupon, -6);
    }

    if (coupon == settlement) {
        coupon = addMonths360(coupon, 6);
    } else if (coupon < settlement) {
        coupon = addMonths360(coupon, 6);
    }

    if (coupon > maturity)
        return QDate();

    ok = true;
    return coupon;
}

QDate previousCouponDate(const QDate &nextCoupon, const QDate &maturity) {
    if (nextCoupon == maturity)
        return addMonths360(nextCoupon, -6);
    return addMonths360(nextCoupon, -6);
}

int countCoupons(const QDate &firstCoupon, const QDate &maturity) {
    int count = 0;
    QDate d = firstCoupon;
    while (d <= maturity) {
        ++count;
        d = addMonths360(d, 6);
    }
    return count;
}
}

// Bond price with 30/360 day count and semiannual coupons.
double Backend::bondPrice() const {
    const QDate settlement = parseDate(m_stack[0]);
    const QDate maturity = parseDate(m_stack[1]);
    if (!settlement.isValid() || !maturity.isValid()) { return 0; }

    bool ok = false;
    const QDate nextCoupon = nextCouponDate(settlement, maturity, ok);
    if (!ok) return 0;

    const QDate prevCoupon = previousCouponDate(nextCoupon, maturity);
    const int dsc = days360(settlement, nextCoupon);
    const int e = days360(prevCoupon, nextCoupon);
    if (e <= 0) return 0;

    const int n = countCoupons(nextCoupon, maturity);
    const double c = m_pmt / 200.0;  // annual percent -> semiannual decimal
    const double y = m_i / 200.0;
    if (nearZero(y)) return 100.0 + c * (n + double(dsc) / e);

    const double periods = n + double(dsc) / e;
    const double base = std::pow(1.0 + y, -periods);
    const double dirty = 100.0 * base + (c / y) * (1.0 - base);
    const double accrued = c * double(e - dsc) / e;
    return dirty - accrued;
}

double Backend::bondYield() const {
    const QDate settlement = parseDate(m_stack[0]);
    const QDate maturity = parseDate(m_stack[1]);
    if (!settlement.isValid() || !maturity.isValid()) { return 0; }

    bool ok = false;
    const QDate nextCoupon = nextCouponDate(settlement, maturity, ok);
    if (!ok) return 0;

    const QDate prevCoupon = previousCouponDate(nextCoupon, maturity);
    const int dsc = days360(settlement, nextCoupon);
    const int e = days360(prevCoupon, nextCoupon);
    if (e <= 0) return 0;

    const int n = countCoupons(nextCoupon, maturity);
    const double c = m_pmt / 200.0;
    const double price = currentX();
    double y = m_i / 200.0;
    if (nearZero(y)) y = 0.05;

    for (int iter = 0; iter < 100; ++iter) {
        const double periods = n + double(dsc) / e;
        const double base = std::pow(1.0 + y, -periods);
        const double dirty = 100.0 * base + (c / y) * (1.0 - base);
        const double accrued = c * double(e - dsc) / e;
        const double f = dirty - accrued - price;

        const double yPlus = y + 1e-8;
        const double basePlus = std::pow(1.0 + yPlus, -periods);
        const double dirtyPlus = 100.0 * basePlus + (c / yPlus) * (1.0 - basePlus);
        const double fPlus = dirtyPlus - accrued - price;

        const double df = (fPlus - f) / 1e-8;
        if (nearZero(df)) break;
        const double delta = f / df;
        y -= delta;
        if (std::abs(delta) < 1e-12) break;
    }
    return y * 200.0;
}

// Static number parsing.
double Backend::toDouble(const QString &text, bool *ok) {
    bool localOk = false;
    if (!ok) ok = &localOk;

    QString normalized = text;
    normalized.replace(QChar(0x2212), QStringLiteral("-"));
    normalized.replace(QLatin1Char(','), QLatin1Char('.'));

    double value = QLocale::c().toDouble(normalized, ok);
    return value;
}

QString Backend::formatForDisplay(double value) const {
    if (!std::isfinite(value))
        return QStringLiteral("Error");

    if (value == 0)
        value = 0;

    if (m_displayMode == DisplayMode::FIX) {
        const QString formatted = QString::number(value, 'f', m_displayDigits);
        const QString mantissa = formatted.contains(QLatin1Char('.'))
                                     ? formatted.section(QLatin1Char('.'), 0, 0)
                                     : formatted;
        if (mantissa.size() > 11 || (formatted.size() > 12 && !formatted.contains(QLatin1Char('e')))) {
            return QString::number(value, 'e', m_displayDigits);
        }
        return formatted;
    }

    if (m_displayMode == DisplayMode::SCI) {
        return QString::number(value, 'e', m_displayDigits);
    }

    if (m_displayMode == DisplayMode::ENG) {
        const int exp = std::floor(std::log10(std::abs(value)) / 3.0) * 3;
        const double mant = value / std::pow(10.0, exp);
        return QString::number(mant, 'f', m_displayDigits) + QLatin1Char('e') + QString::number(exp);
    }

    return QString::number(value, 'g', 10);
}

QString Backend::localizeNumber(const QString &number) const {
    if (m_decimalSeparator == QStringLiteral("."))
        return number;
    QString localized = number;
    return localized.replace(QLatin1Char('.'), m_decimalSeparator);
}

void Backend::copyResult() const {
    if (QClipboard *clipboard = QGuiApplication::clipboard())
        clipboard->setText(display());
}

void Backend::pasteNumber() {
    const QClipboard *clipboard = QGuiApplication::clipboard();
    if (!clipboard)
        return;

    QString text = clipboard->text().trimmed();
    text.replace(QChar(0x2212), QStringLiteral("-"));
    text.replace(QLatin1Char(','), QLatin1Char('.'));
    text.remove(QLatin1Char(' '));

    bool ok = false;
    double value = toDouble(text, &ok);
    if (!ok || !std::isfinite(value))
        return;

    if (m_error)
        pressClear();
    m_entry = formatForDisplay(value);
    m_entryReplace = false;
    emit stateChanged();
}

QVariantMap Backend::windowGeometry() const {
    const QSettings settings;
    const QRect geometry = settings.value(windowGeometrySetting).toRect();
    QVariantMap map;
    map.insert(QStringLiteral("valid"), geometry.isValid());
    map.insert(QStringLiteral("x"), geometry.x());
    map.insert(QStringLiteral("y"), geometry.y());
    map.insert(QStringLiteral("width"), geometry.width());
    map.insert(QStringLiteral("height"), geometry.height());
    map.insert(QStringLiteral("maximized"),
               settings.value(windowMaximizedSetting, false).toBool());
    return map;
}

void Backend::saveWindowGeometry(int x, int y, int width, int height, bool maximized) {
    QSettings settings;
    settings.setValue(windowGeometrySetting, QRect(x, y, width, height));
    settings.setValue(windowMaximizedSetting, maximized);
}

void Backend::setDarkMode(bool darkMode) {
    if (m_darkMode == darkMode)
        return;
    m_darkMode = darkMode;
    loadOmarchyTheme();
    emit darkModeChanged();
}

void Backend::setTextScale(qreal textScale) {
    if (qFuzzyCompare(m_textScale, textScale))
        return;
    m_textScale = textScale;
    emit textScaleChanged();
}

void Backend::loadOmarchyTheme() {
    m_themeBackground = m_darkMode ? QStringLiteral("#101010") : QStringLiteral("#ffffff");
    m_themeForeground = m_darkMode ? QStringLiteral("#eeeeee") : QStringLiteral("#222324");
    m_themeAccent = m_darkMode ? QStringLiteral("#5584aa") : QStringLiteral("#2077b2");
    m_themeSelection = m_darkMode ? QStringLiteral("#186a9a") : QStringLiteral("#2077b2");

    const QString colorsPath = QDir::homePath()
        + QStringLiteral("/.local/state/omarchy/current/theme/colors.toml");
    QString themeMode;
    QFile file(colorsPath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        while (!in.atEnd()) {
            const QString line = in.readLine().trimmed();
            if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
                continue;

            const int equals = line.indexOf(QLatin1Char('='));
            if (equals < 0)
                continue;

            const QString key = line.left(equals).trimmed();
            QString value = line.mid(equals + 1).trimmed();
            if (value.size() >= 2
                    && ((value.front() == QLatin1Char('"') && value.back() == QLatin1Char('"'))
                        || (value.front() == QLatin1Char('\'') && value.back() == QLatin1Char('\''))))
                value = value.mid(1, value.size() - 2);

            if (key == QStringLiteral("mode"))
                themeMode = value;
            else if (key == QStringLiteral("background"))
                m_themeBackground = value;
            else if (key == QStringLiteral("foreground"))
                m_themeForeground = value;
            else if (key == QStringLiteral("accent"))
                m_themeAccent = value;
            else if (key == QStringLiteral("selection"))
                m_themeSelection = value;
        }
    }

    bool themeModeKnown = false;
    bool themeIsDark = m_darkMode;
    if (themeMode == QStringLiteral("dark")) {
        themeIsDark = true;
        themeModeKnown = true;
    } else if (themeMode == QStringLiteral("light")) {
        themeIsDark = false;
        themeModeKnown = true;
    } else {
        const QColor background(m_themeBackground);
        if (background.isValid()) {
            const double luminance = 0.299 * background.redF()
                + 0.587 * background.greenF() + 0.114 * background.blueF();
            themeIsDark = luminance < 0.5;
            themeModeKnown = true;
        }
    }
    if (themeModeKnown && themeIsDark != m_darkMode) {
        m_darkMode = themeIsDark;
        emit darkModeChanged();
    }

    emit themeColorsChanged();
}

void Backend::watchOmarchyTheme() {
    const QStringList watched = m_themeWatcher.files() + m_themeWatcher.directories();
    if (!watched.isEmpty())
        m_themeWatcher.removePaths(watched);

    const QString currentDir = QDir::homePath()
        + QStringLiteral("/.local/state/omarchy/current");
    const QString themeDir = currentDir + QStringLiteral("/theme");
    const QString colorsPath = themeDir + QStringLiteral("/colors.toml");

    if (QDir(currentDir).exists())
        m_themeWatcher.addPath(currentDir);
    if (QDir(themeDir).exists())
        m_themeWatcher.addPath(themeDir);
    if (QFile::exists(colorsPath))
        m_themeWatcher.addPath(colorsPath);
}

void Backend::loadDisplaySettings() {
    const QSettings settings;
    const QString separator = settings.value(QStringLiteral("display/decimalSeparator"),
                                              QStringLiteral(".")).toString().trimmed();
    if (separator == QStringLiteral(",") || separator.compare(QStringLiteral("comma"), Qt::CaseInsensitive) == 0)
        m_decimalSeparator = QStringLiteral(",");
    else
        m_decimalSeparator = QStringLiteral(".");
}

// Programming.
void Backend::pressGtoDigit(const QString &key) {
    if (!isNumberKey(key)) {
        m_prefix = Prefix::None;
        m_gtoFirstDigit = -1;
        return;
    }

    if (m_gtoFirstDigit < 0) {
        m_gtoFirstDigit = key.toInt();
        return;
    }

    const int target = m_gtoFirstDigit * 10 + key.toInt();
    m_gtoFirstDigit = -1;
    m_prefix = Prefix::None;

    if (m_programMode) {
        recordStep(QStringLiteral("GTO"));
        recordStep(QString::number(target));
    } else {
        goToStep(target);
    }
}

void Backend::recordStep(const QString &key) {
    if (m_programCounter >= 100) { m_error = true; return; }
    if (m_programCounter >= static_cast<int>(m_programSteps.size()))
        m_programSteps.resize(m_programCounter + 1);
    m_programSteps[m_programCounter] = key;
    ++m_programCounter;
    m_entryReplace = true;
}

void Backend::executeProgram() {
    if (m_programRunning) { m_programRunning = false; return; }
    m_programRunning = true;
    while (m_programRunning && m_programCounter < static_cast<int>(m_programSteps.size())) {
        const QString step = m_programSteps[m_programCounter];
        ++m_programCounter;

        if (step == QStringLiteral("x≤y")) {
            if (m_stack[0] > m_stack[1]) {
                if (m_programCounter < static_cast<int>(m_programSteps.size()))
                    ++m_programCounter;
            }
            continue;
        }
        if (step == QStringLiteral("x=0")) {
            if (!nearZero(m_stack[0])) {
                if (m_programCounter < static_cast<int>(m_programSteps.size()))
                    ++m_programCounter;
            }
            continue;
        }

        if (step == QStringLiteral("GTO")) {
            if (m_programCounter < static_cast<int>(m_programSteps.size())) {
                const QString target = m_programSteps[m_programCounter];
                ++m_programCounter;
                bool ok = false;
                const int t = target.toInt(&ok);
                if (ok) goToStep(t);
            }
            continue;
        }

        pressKey(step);

        if (step == QStringLiteral("PSE")) {
            m_programRunning = false;
            break;
        }
    }
    m_programRunning = false;
    emit stateChanged();
}

void Backend::singleStep() {
    if (m_programCounter >= static_cast<int>(m_programSteps.size())) return;
    const QString step = m_programSteps[m_programCounter];
    ++m_programCounter;
    if (step == QStringLiteral("x≤y") || step == QStringLiteral("x=0")) {
        // no-op
    } else if (step == QStringLiteral("PSE")) {
        // pause
    } else if (step == QStringLiteral("GTO")) {
        if (m_programCounter < static_cast<int>(m_programSteps.size())) {
            const QString target = m_programSteps[m_programCounter];
            ++m_programCounter;
            bool ok = false;
            const int t = target.toInt(&ok);
            if (ok) goToStep(t);
        }
    } else {
        pressKey(step);
    }
    emit stateChanged();
}

void Backend::goToStep(int step) {
    if (step < 0 || step > 99) return;
    m_programCounter = step;
}

void Backend::clearProgram() {
    m_programSteps.clear();
    m_programCounter = 0;
    m_programRunning = false;
    m_entryReplace = true;
}

QString Backend::programKeycode(const QString &step) const {
    // HP-12C-like keycodes: prefix 42 = f, 43 = g, 44 = STO, 45 = RCL,
    // followed by the physical key keycode.  Plain keys use their own code.
    static const QHash<QString, QString> codes = []() {
        QHash<QString, QString> map;
        map.insert(QStringLiteral("n"), QStringLiteral("11"));
        map.insert(QStringLiteral("i"), QStringLiteral("12"));
        map.insert(QStringLiteral("PV"), QStringLiteral("13"));
        map.insert(QStringLiteral("PMT"), QStringLiteral("14"));
        map.insert(QStringLiteral("FV"), QStringLiteral("15"));
        map.insert(QStringLiteral("CHS"), QStringLiteral("16"));
        map.insert(QStringLiteral("7"), QStringLiteral("17"));
        map.insert(QStringLiteral("8"), QStringLiteral("18"));
        map.insert(QStringLiteral("9"), QStringLiteral("19"));
        map.insert(QStringLiteral("y^x"), QStringLiteral("21"));
        map.insert(QStringLiteral("1/x"), QStringLiteral("22"));
        map.insert(QStringLiteral("%T"), QStringLiteral("23"));
        map.insert(QStringLiteral("Δ%"), QStringLiteral("24"));
        map.insert(QStringLiteral("%"), QStringLiteral("25"));
        map.insert(QStringLiteral("EEX"), QStringLiteral("26"));
        map.insert(QStringLiteral("R/S"), QStringLiteral("31"));
        map.insert(QStringLiteral("SST"), QStringLiteral("32"));
        map.insert(QStringLiteral("R↓"), QStringLiteral("33"));
        map.insert(QStringLiteral("x<>y"), QStringLiteral("34"));
        map.insert(QStringLiteral("CLx"), QStringLiteral("35"));
        map.insert(QStringLiteral("ENTER"), QStringLiteral("36"));
        map.insert(QStringLiteral("4"), QStringLiteral("37"));
        map.insert(QStringLiteral("5"), QStringLiteral("38"));
        map.insert(QStringLiteral("6"), QStringLiteral("39"));
        map.insert(QStringLiteral("×"), QStringLiteral("41"));
        map.insert(QStringLiteral("÷"), QStringLiteral("42"));
        map.insert(QStringLiteral("+"), QStringLiteral("43"));
        map.insert(QStringLiteral("−"), QStringLiteral("44"));
        map.insert(QStringLiteral("Σ+"), QStringLiteral("45"));
        map.insert(QStringLiteral("1"), QStringLiteral("46"));
        map.insert(QStringLiteral("2"), QStringLiteral("47"));
        map.insert(QStringLiteral("3"), QStringLiteral("48"));
        map.insert(QStringLiteral("0"), QStringLiteral("49"));
        map.insert(QStringLiteral("."), QStringLiteral("50"));
        map.insert(QStringLiteral("STO"), QStringLiteral("44 00"));
        map.insert(QStringLiteral("RCL"), QStringLiteral("45 00"));
        map.insert(QStringLiteral("GTO"), QStringLiteral("43 33"));
        return map;
    }();

    static const QHash<QString, QString> prefixed = []() {
        QHash<QString, QString> map;
        map.insert(QStringLiteral("CLEAR FIN"), QStringLiteral("42 11"));
        map.insert(QStringLiteral("12x"), QStringLiteral("43 11"));
        map.insert(QStringLiteral("INT"), QStringLiteral("42 12"));
        map.insert(QStringLiteral("12÷"), QStringLiteral("43 12"));
        map.insert(QStringLiteral("NPV"), QStringLiteral("42 13"));
        map.insert(QStringLiteral("CFo"), QStringLiteral("43 13"));
        map.insert(QStringLiteral("AMORT"), QStringLiteral("42 14"));
        map.insert(QStringLiteral("CFj"), QStringLiteral("43 14"));
        map.insert(QStringLiteral("IRR"), QStringLiteral("42 15"));
        map.insert(QStringLiteral("Nj"), QStringLiteral("43 15"));
        map.insert(QStringLiteral("DATE"), QStringLiteral("43 16"));
        map.insert(QStringLiteral("BEG"), QStringLiteral("43 17"));
        map.insert(QStringLiteral("END"), QStringLiteral("43 18"));
        map.insert(QStringLiteral("MEM"), QStringLiteral("43 19"));
        map.insert(QStringLiteral("PRICE"), QStringLiteral("42 21"));
        map.insert(QStringLiteral("√x"), QStringLiteral("43 21"));
        map.insert(QStringLiteral("YTM"), QStringLiteral("42 22"));
        map.insert(QStringLiteral("e^x"), QStringLiteral("43 22"));
        map.insert(QStringLiteral("SL"), QStringLiteral("42 23"));
        map.insert(QStringLiteral("LN"), QStringLiteral("43 23"));
        map.insert(QStringLiteral("SOYD"), QStringLiteral("42 24"));
        map.insert(QStringLiteral("FRAC"), QStringLiteral("43 24"));
        map.insert(QStringLiteral("DB"), QStringLiteral("42 25"));
        map.insert(QStringLiteral("INTG"), QStringLiteral("43 25"));
        map.insert(QStringLiteral("ΔDYS"), QStringLiteral("43 26"));
        map.insert(QStringLiteral("P/R"), QStringLiteral("42 31"));
        map.insert(QStringLiteral("PSE"), QStringLiteral("43 31"));
        map.insert(QStringLiteral("CLEAR Σ"), QStringLiteral("42 32"));
        map.insert(QStringLiteral("BST"), QStringLiteral("43 32"));
        map.insert(QStringLiteral("CLEAR PRGM"), QStringLiteral("42 33"));
        map.insert(QStringLiteral("x≤y"), QStringLiteral("43 34"));
        map.insert(QStringLiteral("CLEAR REG"), QStringLiteral("42 35"));
        map.insert(QStringLiteral("x=0"), QStringLiteral("43 35"));
        map.insert(QStringLiteral("CLEAR PREFIX"), QStringLiteral("42 36"));
        map.insert(QStringLiteral("LSTx"), QStringLiteral("43 36"));
        map.insert(QStringLiteral("D.MY"), QStringLiteral("43 37"));
        map.insert(QStringLiteral("M.DY"), QStringLiteral("43 38"));
        map.insert(QStringLiteral("x↔w"), QStringLiteral("43 39"));
        map.insert(QStringLiteral("x̂,r"), QStringLiteral("43 46"));
        map.insert(QStringLiteral("ŷ,r"), QStringLiteral("43 47"));
        map.insert(QStringLiteral("n!"), QStringLiteral("43 48"));
        map.insert(QStringLiteral("x̄"), QStringLiteral("43 49"));
        map.insert(QStringLiteral("s"), QStringLiteral("43 50"));
        map.insert(QStringLiteral("Σ−"), QStringLiteral("43 45"));
        return map;
    }();

    if (prefixed.contains(step))
        return prefixed.value(step);
    if (step.startsWith(QStringLiteral("RCL ")))
        return QStringLiteral("45 ") + step.mid(4).rightJustified(2, QLatin1Char('0'));
    if (step.startsWith(QStringLiteral("STO ")))
        return QStringLiteral("44 ") + step.mid(4).rightJustified(2, QLatin1Char('0'));
    if (step.startsWith(QStringLiteral("GTO ")))
        return step.mid(4);
    if (codes.contains(step))
        return codes.value(step);
    return step;
}

bool Backend::conditionalSkip(const QString &test) {
    if (test == QStringLiteral("x≤y"))
        return m_stack[0] > m_stack[1];
    if (test == QStringLiteral("x=0"))
        return !nearZero(m_stack[0]);
    return false;
}
