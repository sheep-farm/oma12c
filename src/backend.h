#pragma once

#include <QDate>
#include <QFileSystemWatcher>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <array>
#include <unordered_map>
#include <vector>

class Backend : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString display READ display NOTIFY stateChanged)
    Q_PROPERTY(QString expression READ expression NOTIFY stateChanged)
    Q_PROPERTY(QString decimalSeparator READ decimalSeparator CONSTANT)
    Q_PROPERTY(bool darkMode READ darkMode WRITE setDarkMode NOTIFY darkModeChanged)
    Q_PROPERTY(qreal textScale READ textScale WRITE setTextScale NOTIFY textScaleChanged)
    Q_PROPERTY(QString themeBackground READ themeBackground NOTIFY themeColorsChanged)
    Q_PROPERTY(QString themeForeground READ themeForeground NOTIFY themeColorsChanged)
    Q_PROPERTY(QString themeAccent READ themeAccent NOTIFY themeColorsChanged)
    Q_PROPERTY(QString themeSelection READ themeSelection NOTIFY themeColorsChanged)

    // HP-12C-style annunciators exposed to the UI.
    Q_PROPERTY(QString prefix READ prefix NOTIFY stateChanged)
    Q_PROPERTY(bool beginMode READ beginMode NOTIFY stateChanged)
    Q_PROPERTY(bool dmyMode READ dmyMode NOTIFY stateChanged)
    Q_PROPERTY(bool programMode READ programMode NOTIFY stateChanged)

public:
    explicit Backend(QObject *parent = nullptr);

    QString display() const;
    QString expression() const;
    QString decimalSeparator() const { return m_decimalSeparator; }
    QString prefix() const;

    bool darkMode() const { return m_darkMode; }
    void setDarkMode(bool darkMode);
    qreal textScale() const { return m_textScale; }
    void setTextScale(qreal textScale);
    QString themeBackground() const { return m_themeBackground; }
    QString themeForeground() const { return m_themeForeground; }
    QString themeAccent() const { return m_themeAccent; }
    QString themeSelection() const { return m_themeSelection; }

    bool beginMode() const { return m_beginMode; }
    bool dmyMode() const { return m_dmyMode; }
    bool programMode() const { return m_programMode; }

    static double toDouble(const QString &text, bool *ok = nullptr);
    QString formatForDisplay(double value) const;
    QString localizeNumber(const QString &number) const;

    Q_INVOKABLE void pressKey(const QString &key);
    Q_INVOKABLE void copyResult() const;
    Q_INVOKABLE void pasteNumber();
    Q_INVOKABLE QVariantMap windowGeometry() const;
    Q_INVOKABLE void saveWindowGeometry(int x, int y, int width, int height, bool maximized);

    // Testing and internal helpers.
    double x() const { return m_stack[0]; }
    double y() const { return m_stack[1]; }
    double z() const { return m_stack[2]; }
    double t() const { return m_stack[3]; }
    double lastX() const { return m_lastX; }
    double financialRegister(const QString &name) const;
    double storageRegister(int index) const;
    int displayDigits() const { return m_displayDigits; }
    int displayMode() const { return static_cast<int>(m_displayMode); }

signals:
    void stateChanged();
    void darkModeChanged();
    void textScaleChanged();
    void themeColorsChanged();

private:
    enum class Prefix {
        None,
        f,
        g,
        STO,
        STOop, // STO followed by an arithmetic operator
        RCL,
        GTO,
        FIX,
    };

    enum class DisplayMode {
        FIX,
        SCI,
        ENG,
    };

    void pressDigit(const QString &digit);
    void pressDecimal();
    void pressEnter();
    void pressChs();
    void pressEex();
    void pressOperator(const QString &op);
    void pressStackOp(const QString &op);
    void pressClear();
    void pressClx();

    void pressFinancialKey(const QString &key);
    void pressStorageKey(const QString &key);
    void pressRecallKey(const QString &key);
    void executePrefixedKey(const QString &key);
    void setPrefix(Prefix prefix);
    void liftStack();
    void dropStack();
    void rollDown();
    void swapXY();
    void lastXToX();
    double currentX() const;

    // Programming.
    QString recordableKey(const QString &key) const;
    void recordStep(const QString &key);
    void pressGtoDigit(const QString &key);
    void executeProgram();
    void singleStep();
    void goToStep(int step);
    void clearProgram();
    bool conditionalSkip(const QString &test);
    double parseEntry() const;
    bool hasPendingEntry() const;
    void commitEntry();
    void clearEntry();
    bool appendToEntry(const QString &text);

    // Financial mathematics.
    void clearAllRegisters();
    void clearFinancialRegisters();
    double solveTvm(const QString &variable) const;
    double calculateFv() const;
    double calculatePv() const;
    double calculatePmt() const;
    double calculateN() const;
    double calculateI() const;
    double tvmFactor() const;
    void amortize();

    // Cash flow analysis.
    void setCashFlowFirst();
    void setCashFlowNext();
    void setCashFlowCount();
    double computeNpv() const;
    double computeIrr() const;
    double straightLineDepreciation() const;
    double sumOfYearsDigitsDepreciation() const;
    double decliningBalanceDepreciation() const;

    // Date arithmetic.
    QDate parseDate(double value) const;
    double encodeDate(const QDate &date) const;
    void computeDaysBetween();
    void computeFutureDate();

    // Bonds (simplified, semiannual 30/360).
    double bondPrice() const;
    double bondYield() const;

    // Statistics.
    void clearStatistics();
    void accumulateSigma(bool add);
    void computeMeanX();
    void computeStdDevS();
    void linearEstimateX();
    void linearEstimateY();
    void weightedAverage();

    // Utility.
    void roundToDisplay();
    void simpleInterest();

    // Percent.
    void percent();
    void percentChange();
    void percentTotal();

    // Theme / settings.
    void loadOmarchyTheme();
    void watchOmarchyTheme();
    void loadDisplaySettings();

    // Stack and registers.
    std::array<double, 4> m_stack = {0, 0, 0, 0};
    double m_lastX = 0;
    std::unordered_map<int, double> m_storageRegisters;
    double m_n = 0, m_i = 0, m_pv = 0, m_pmt = 0, m_fv = 0;

    // Cash flow analysis.
    std::vector<double> m_cashFlows;
    std::vector<int> m_cashFlowCounts;
    int m_cashFlowIndex = 0;

    // Number entry.
    QString m_entry;
    bool m_entryHasDecimal = false;
    bool m_entryHasExponent = false;
    bool m_entryExponentHasSign = false;
    bool m_entryIsNegative = false;
    bool m_entryReplace = true; // Next digit replaces the displayed value.

    // Calculator state.
    Prefix m_prefix = Prefix::None;
    Prefix m_secondaryPrefix = Prefix::None;
    DisplayMode m_displayMode = DisplayMode::FIX;
    int m_displayDigits = 2;
    bool m_beginMode = false;
    bool m_dmyMode = false;
    bool m_programMode = false;
    bool m_error = false;
    QString m_stoOp;
    int m_gtoFirstDigit = -1;

    // Program memory.
    std::vector<QString> m_programSteps;
    int m_programCounter = 0;
    int m_programReturn = 0;
    bool m_programRunning = false;

    // Theme.
    bool m_darkMode = true;
    qreal m_textScale = 1.0;
    QString m_decimalSeparator = QStringLiteral(".");
    QString m_themeBackground;
    QString m_themeForeground;
    QString m_themeAccent;
    QString m_themeSelection;
    QFileSystemWatcher m_themeWatcher;
};
