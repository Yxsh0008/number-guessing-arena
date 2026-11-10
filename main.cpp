/*
 * ===========================================================================
 *  NUMBER GUESSING ARENA  (OOPS in C++ - PBL / Mini Project)
 * ===========================================================================
 *  OOP concepts used:
 *    Abstraction ........ abstract base class GameMode
 *    Inheritance ........ ClassicMode, ReverseMode, DuelMode derive from it
 *    Polymorphism ....... Arena calls mode->play() without knowing the mode
 *    Encapsulation ...... SecretNumber hides the secret; Range validates
 *    Operator overload .. Range <<, GuessRecord <<, Stats +=
 *    Templates .......... Statistics<T> (average / best of any numeric type)
 *    Exceptions ......... GameException hierarchy
 *    Smart pointers ..... unique_ptr<GameMode>
 *    STL ................ vector, algorithm, cmath, random
 *    File handling ...... guess_scores.txt (high scores)
 *
 *  UNIQUE FEATURE  ->  "BINARY-SEARCH COACH"
 *    1. After every guess the coach shows the range still possible.
 *    2. 'hint' gives the ideal next guess (middle of the range) - 2 per game.
 *    3. After the game it analyses EVERY guess: how much of the range it
 *       eliminated compared with the ideal 50%, and rates your efficiency
 *       against the optimal number of guesses  ceil(log2 N).
 *    4. Reverse mode: the computer guesses YOUR number with binary search and
 *       catches contradicting answers.
 *    5. Duel mode: race against the binary-search computer.
 * ===========================================================================
 */

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <numeric>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace std;

// ---------------------------------------------------------------- exceptions
class GameException : public runtime_error {
public:
    explicit GameException(const string& m) : runtime_error(m) {}
};
class InvalidGuessException : public GameException {
public:
    explicit InvalidGuessException(const string& m) : GameException(m) {}
};
class InconsistentAnswerException : public GameException {
public:
    explicit InconsistentAnswerException(const string& m) : GameException(m) {}
};
class FileException : public GameException {
public:
    explicit FileException(const string& m) : GameException(m) {}
};

mt19937 rng;

// ======================================================= Difficulty and Range
struct Difficulty {
    string name;
    int low, high, maxAttempts;
    int optimal() const { return static_cast<int>(ceil(log2(high - low + 1))); }   // best possible worst-case guesses
};
const Difficulty LEVELS[3] = {{"Easy", 1, 50, 8}, {"Medium", 1, 100, 9}, {"Hard", 1, 1000, 12}};

class Range {
    int lo, hi;

public:
    Range(int lo, int hi) : lo(lo), hi(hi) {
        if (lo > hi) throw InconsistentAnswerException("The range became empty - the answers contradict each other.");
    }
    int low() const { return lo; }
    int high() const { return hi; }
    int size() const { return hi - lo + 1; }
    int middle() const { return lo + (hi - lo) / 2; }
    bool contains(int x) const { return x >= lo && x <= hi; }
    friend ostream& operator<<(ostream& os, const Range& r) { return os << "[" << r.lo << " .. " << r.hi << "]"; }
};

// ====================================================== secret + guess records
class SecretNumber {
    int value;

public:
    SecretNumber(int lo, int hi) : value(uniform_int_distribution<int>(lo, hi)(rng)) {}
    int compare(int guess) const { return guess < value ? -1 : guess > value ? 1 : 0; }   // -1: too low
    int reveal() const { return value; }
};

struct GuessRecord {
    int guess;
    int verdict;            // -1 too low, 0 correct, 1 too high
    int rangeBefore;        // size of the range before this guess
    int rangeAfter;         // size after
    friend ostream& operator<<(ostream& os, const GuessRecord& g) {
        return os << g.guess << (g.verdict < 0 ? " (too low)" : g.verdict > 0 ? " (too high)" : " (correct)");
    }
};

// =========================================================== template Statistics<T>
template <typename T>
class Statistics {
    vector<T> values;

public:
    Statistics& operator+=(const T& v) { values.push_back(v); return *this; }       // operator overloading
    size_t count() const { return values.size(); }
    double average() const { return values.empty() ? 0.0 : accumulate(values.begin(), values.end(), 0.0) / values.size(); }
    T best() const { return *min_element(values.begin(), values.end()); }
    T worst() const { return *max_element(values.begin(), values.end()); }
};

struct GameResult {
    bool won = false;
    int attempts = 0;
    int score = 0;
    string mode;
};

// ======================================================= UNIQUE FEATURE: the coach
class Coach {
public:
    static void report(const Difficulty& lv, const vector<GuessRecord>& rec, bool won, int hintsUsed, int& scoreOut) {
        cout << "\n=============== BINARY-SEARCH COACH ===============\n"
             << "Range " << lv.low << ".." << lv.high << " (" << lv.high - lv.low + 1 << " numbers)   Optimal worst case: "
             << lv.optimal() << " guesses\n\n"
             << left << setw(7) << "Guess" << setw(10) << "Number" << setw(14) << "Range before" << setw(12) << "Eliminated" << "Quality\n"
             << string(55, '-') << "\n";
        int good = 0;
        for (size_t i = 0; i < rec.size(); ++i) {
            double elim = rec[i].rangeBefore > 0 ? 100.0 * (rec[i].rangeBefore - rec[i].rangeAfter) / rec[i].rangeBefore : 0.0;
            if (rec[i].verdict == 0) elim = 100.0;
            string q = elim >= 40 ? "Excellent" : elim >= 25 ? "Good" : "Weak";
            if (elim >= 40) ++good;
            cout << left << setw(7) << i + 1 << setw(10) << rec[i].guess << setw(14) << rec[i].rangeBefore
                 << fixed << setprecision(0) << setw(12) << (to_string(static_cast<int>(elim)) + "%") << q << "\n";
        }
        cout << string(55, '-') << "\n";
        if (!won) {
            scoreOut = 0;
            cout << "Not found this time. A perfect binary search finds ANY number in " << lv.optimal() << " guesses: always guess the middle of what is left.\n";
        } else {
            int attempts = static_cast<int>(rec.size());
            double eff = 100.0 * lv.optimal() / max(attempts, lv.optimal());
            scoreOut = max(0, static_cast<int>(round(eff)) - 15 * hintsUsed);
            string title = eff >= 95 ? "Binary Master" : eff >= 75 ? "Sharp Searcher" : eff >= 55 ? "Getting There" : "Guess Rookie";
            cout << "Attempts: " << attempts << "   Optimal: " << lv.optimal() << "   Efficiency: " << eff << "%\n"
                 << "Strong guesses (>= 40% eliminated): " << good << " of " << attempts << "\n"
                 << "Hints used: " << hintsUsed << " (-15 points each)   SCORE: " << scoreOut << "   Title: " << title << "\n";
        }
        cout << "===================================================\n";
    }
};

// ============================================================ GameMode (abstract)
class GameMode {
public:
    virtual ~GameMode() {}
    virtual string name() const = 0;
    virtual GameResult play(const Difficulty& lv) = 0;
};

string readLine(const string& p) {
    cout << p;
    string s;
    if (!getline(cin, s)) throw GameException("Input ended.");
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == string::npos) return "";
    return s.substr(a, s.find_last_not_of(" \t\r\n") - a + 1);
}

// ---------------------------------------------------------------- classic mode
class ClassicMode : public GameMode {
public:
    string name() const override { return "Classic (you guess the computer's number)"; }
    GameResult play(const Difficulty& lv) override {
        SecretNumber secret(lv.low, lv.high);
        Range range(lv.low, lv.high);
        vector<GuessRecord> rec;
        int hints = 2, hintsUsed = 0;
        GameResult res;
        res.mode = "Classic";
        cout << "\nI am thinking of a number between " << lv.low << " and " << lv.high << ". You have " << lv.maxAttempts
             << " attempts.  Type a number, or 'hint' (" << hints << " left), or 'q' to give up.\n";
        bool won = false;
        while (static_cast<int>(rec.size()) < lv.maxAttempts) {
            string in = readLine("Attempt " + to_string(rec.size() + 1) + "/" + to_string(lv.maxAttempts) + " (range " + to_string(range.size()) + " numbers) > ");
            if (in == "q") break;
            if (in == "hint") {
                if (hints == 0) { cout << "  No hints left.\n"; continue; }
                --hints; ++hintsUsed;
                cout << "  COACH: the ideal guess is " << range.middle() << " (the middle of " << range << "). It removes half of the possibilities.\n";
                continue;
            }
            int g;
            try {
                size_t pos;
                g = stoi(in, &pos);
                if (pos != in.size()) throw invalid_argument("x");
            } catch (const invalid_argument&) { cout << "  [x] Please type a whole number, 'hint' or 'q'.\n"; continue; }
            catch (const out_of_range&) { cout << "  [x] That number is far too large.\n"; continue; }
            try {
                if (g < lv.low || g > lv.high) throw InvalidGuessException("Guess must be between " + to_string(lv.low) + " and " + to_string(lv.high) + ".");
                if (!range.contains(g)) cout << "  Coach: " << g << " is outside the range you already know (" << range << ") - a wasted guess.\n";
            } catch (const InvalidGuessException& e) { cout << "  [x] " << e.what() << "\n"; continue; }

            int v = secret.compare(g);
            int before = range.size();
            if (range.contains(g)) {
                if (v < 0) range = Range(g + 1, range.high());
                else if (v > 0) range = Range(range.low(), g - 1);
            }
            int after = (v == 0) ? 0 : range.size();
            rec.push_back({g, v, before, after});
            if (v == 0) { won = true; cout << "  *** CORRECT in " << rec.size() << " attempt(s)! ***\n"; break; }
            cout << "  " << (v < 0 ? "Too LOW" : "Too HIGH") << ".  Number is now in " << range << "\n";
        }
        if (!won) cout << "  The number was " << secret.reveal() << ".\n";
        res.won = won;
        res.attempts = static_cast<int>(rec.size());
        Coach::report(lv, rec, won, hintsUsed, res.score);
        return res;
    }
};

// ---------------------------------------------------------------- reverse mode
class ReverseMode : public GameMode {
public:
    string name() const override { return "Reverse (the computer guesses YOUR number)"; }
    GameResult play(const Difficulty& lv) override {
        GameResult res;
        res.mode = "Reverse";
        cout << "\nThink of a number between " << lv.low << " and " << lv.high << " and keep it secret.\n"
             << "I will guess. Answer: h = your number is HIGHER, l = LOWER, c = correct.\n";
        Range range(lv.low, lv.high);
        int tries = 0;
        while (true) {
            int g = range.middle();
            ++tries;
            string a;
            while (true) {
                a = readLine("Guess " + to_string(tries) + ": is it " + to_string(g) + "?  (h/l/c) > ");
                if (a == "h" || a == "l" || a == "c") break;
                cout << "  [x] Answer with h, l or c.\n";
            }
            if (a == "c") break;
            try {
                range = (a == "h") ? Range(g + 1, range.high()) : Range(range.low(), g - 1);
            } catch (const InconsistentAnswerException& e) {
                cout << "  [x] " << e.what() << "\n  Caught you! No number fits all your answers.\n";
                res.attempts = tries;
                return res;
            }
            cout << "  Remaining range: " << range << " (" << range.size() << " numbers)\n";
        }
        res.won = true;
        res.attempts = tries;
        res.score = tries;
        cout << "  I found your number in " << tries << " guesses (the optimal worst case is " << lv.optimal() << ").\n";
        return res;
    }
};

// ------------------------------------------------------------------- duel mode
class DuelMode : public GameMode {
public:
    string name() const override { return "Duel (race the binary-search computer)"; }
    GameResult play(const Difficulty& lv) override {
        SecretNumber secret(lv.low, lv.high);
        Range comp(lv.low, lv.high), me(lv.low, lv.high);
        GameResult res;
        res.mode = "Duel";
        cout << "\nDuel! We both guess the same secret number (" << lv.low << ".." << lv.high << "). You go first. First to find it wins.\n";
        vector<GuessRecord> rec;
        for (int round = 1; round <= lv.maxAttempts; ++round) {
            string in = readLine("Round " + to_string(round) + " - your guess (range " + to_string(me.size()) + " numbers) > ");
            int g;
            try {
                size_t pos;
                g = stoi(in, &pos);
                if (pos != in.size() || g < lv.low || g > lv.high) throw invalid_argument("x");
            } catch (...) { cout << "  [x] Type a whole number between " << lv.low << " and " << lv.high << ".\n"; --round; continue; }
            int v = secret.compare(g);
            int before = me.size();
            if (me.contains(g)) { if (v < 0) me = Range(g + 1, me.high()); else if (v > 0) me = Range(me.low(), g - 1); }
            rec.push_back({g, v, before, v == 0 ? 0 : me.size()});
            if (v == 0) { cout << "  You found it - YOU WIN the duel!\n"; res.won = true; res.attempts = round; res.score = 100 - 10 * (round - 1); break; }
            cout << "  " << (v < 0 ? "Too LOW" : "Too HIGH") << ".\n";
            int cg = comp.middle();
            int cv = secret.compare(cg);
            cout << "  Computer guesses " << cg << " -> " << (cv < 0 ? "too low" : cv > 0 ? "too high" : "CORRECT") << "\n";
            if (cv == 0) { cout << "  The computer found it - COMPUTER WINS.\n"; res.attempts = round; break; }
            comp = cv < 0 ? Range(cg + 1, comp.high()) : Range(comp.low(), cg - 1);
        }
        if (res.attempts == 0) { cout << "  Nobody found it. The number was " << secret.reveal() << ".\n"; res.attempts = lv.maxAttempts; }
        res.score = max(0, res.score);
        return res;
    }
};

// ============================================================ high scores (file)
void saveScore(const string& name, const GameResult& r) {
    ofstream out("guess_scores.txt", ios::app);
    if (!out) throw FileException("Cannot write guess_scores.txt");
    out << name << "|" << r.mode << "|" << r.score << "|" << r.attempts << "\n";
}
vector<vector<string>> loadScores() {
    vector<vector<string>> rows;
    ifstream in("guess_scores.txt");
    string line;
    while (getline(in, line)) {
        vector<string> f;
        string part;
        stringstream ss(line);
        while (getline(ss, part, '|')) f.push_back(part);
        if (f.size() == 4) rows.push_back(f);
    }
    return rows;
}
void showScores() {
    auto rows = loadScores();
    if (rows.empty()) { cout << "No scores yet.\n"; return; }
    sort(rows.begin(), rows.end(), [](auto& a, auto& b) { return stoi(a[2]) > stoi(b[2]); });
    cout << "\n--- TOP SCORES ---\n" << left << setw(12) << "Player" << setw(10) << "Mode" << setw(8) << "Score" << "Attempts\n" << string(40, '-') << "\n";
    for (size_t i = 0; i < rows.size() && i < 5; ++i)
        cout << left << setw(12) << rows[i][0] << setw(10) << rows[i][1] << setw(8) << rows[i][2] << rows[i][3] << "\n";
    Statistics<int> att;
    for (auto& r : rows) if (r[1] == "Classic") att += stoi(r[3]);
    if (att.count()) cout << "Classic games: " << att.count() << "  average attempts " << fixed << setprecision(1) << att.average()
                          << "  best " << att.best() << "  worst " << att.worst() << "\n";
}

// ============================================================ menu
int readInt(const string& p) {
    while (true) {
        string s = readLine(p);
        try {
            size_t pos;
            int v = stoi(s, &pos);
            if (pos != s.size()) throw invalid_argument("x");
            return v;
        } catch (...) { cout << "  Please enter a valid whole number.\n"; }
    }
}

void printMenu() {
    cout << "\n=========== NUMBER GUESSING ARENA ===========\n"
         << "  1. Classic  - you guess the computer's number\n"
         << "  2. Reverse  - the computer guesses your number\n"
         << "  3. Duel     - race the binary-search computer\n"
         << "  4. High scores\n"
         << "  0. Exit\n"
         << "=============================================\n";
}

int main(int argc, char* argv[]) {
    unsigned seed = random_device{}();
    if (argc > 1) seed = static_cast<unsigned>(stoul(argv[1]));
    rng.seed(seed);

    while (true) {
        printMenu();
        int ch;
        try { ch = readInt("Choice: "); } catch (const GameException&) { ch = 0; }
        try {
            if (ch == 0) { cout << "Thanks for playing!\n"; return 0; }
            if (ch == 4) { showScores(); continue; }
            if (ch < 1 || ch > 3) { cout << "Invalid choice.\n"; continue; }

            int lvl = readInt("Difficulty (1 = Easy 1-50, 2 = Medium 1-100, 3 = Hard 1-1000): ");
            if (lvl < 1 || lvl > 3) throw InvalidGuessException("Difficulty must be 1, 2 or 3.");
            const Difficulty& lv = LEVELS[lvl - 1];

            unique_ptr<GameMode> mode;                                   // polymorphism through a base pointer
            if (ch == 1) mode = make_unique<ClassicMode>();
            else if (ch == 2) mode = make_unique<ReverseMode>();
            else mode = make_unique<DuelMode>();

            cout << "\nMode: " << mode->name() << "   Level: " << lv.name << "\n";
            GameResult r = mode->play(lv);
            if (ch != 2 && r.attempts > 0) {
                string name = readLine("Your name for the score board (Enter = Player): ");
                saveScore(name.empty() ? "Player" : name, r);
            } else if (ch == 2 && r.won) saveScore("Computer", r);
        } catch (const GameException& e) {
            cout << "  [x] " << e.what() << "\n";
            if (string(e.what()) == "Input ended.") return 0;
        } catch (const exception& e) {
            cout << "  [x] Unexpected error: " << e.what() << "\n";
        }
    }
}
