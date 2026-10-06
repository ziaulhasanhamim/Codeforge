#include "codeforge/ComplexityAnalyzer.hpp"
#include "codeforge/CallGraph.hpp"
#include "codeforge/ComplexityTree.hpp"
#include "codeforge/Tokenizer.hpp"
#include "codeforge/dsa/MergeSort.hpp"

#include <algorithm>
#include "codeforge/dsa/Stack.hpp"
#include <string>
#include <utility>
#include <vector>

namespace codeforge
{
    enum class Growth
    {
        Constant,
        Logarithmic,
        Linear,
        Unknown
    };

    struct ComplexityValue
    {
        int linearPower = 0;
        int logPower = 0;
        bool unknown = false;
    };

    struct FunctionRange
    {
        std::string name;
        std::size_t bodyStart;
        std::size_t bodyEnd;
    };

    struct LoopInfo
    {
        std::string kind;
        std::size_t start;
        std::size_t bodyStart;
        std::size_t bodyEnd;
        Growth growth;
    };

    struct FunctionReport
    {
        std::string name;
        std::string time;
        std::string space;
        std::string reason;
        int score = 0;
        int graphIndex = -1;
    };

    void addOnce(std::vector<std::string> &items, const std::string &item)
    {
        if (std::find(items.begin(), items.end(), item) == items.end())
        {
            items.push_back(item);
        }
    }

    bool isIdentifier(const Token &token)
    {
        return token.type == TokenType::Identifier;
    }

    bool isNumber(const Token &token, const std::string &text = "")
    {
        return token.type == TokenType::Number &&
               (text.empty() || token.text == text);
    }

    bool contains(const std::vector<Token> &tokens, std::size_t begin,
                  std::size_t end, const std::string &text)
    {
        end = std::min(end, tokens.size());
        for (std::size_t i = begin; i < end; ++i)
        {
            if (tokens[i].text == text)
                return true;
        }
        return false;
    }

    std::vector<int> buildMatchingPairs(const std::vector<Token> &tokens)
    {
        std::vector<int> matching(tokens.size(), -1);
        dsa::Stack<std::size_t> parentheses;
        dsa::Stack<std::size_t> braces;
        dsa::Stack<std::size_t> brackets;

        const auto close = [&matching](dsa::Stack<std::size_t> &openings,
                                       std::size_t closing)
        {
            if (openings.empty())
                return;
            const std::size_t opening = openings.top();
            openings.pop();
            matching[opening] = static_cast<int>(closing);
            matching[closing] = static_cast<int>(opening);
        };

        for (std::size_t i = 0; i < tokens.size(); ++i)
        {
            if (tokens[i].text == "(")
                parentheses.push(i);
            else if (tokens[i].text == ")")
                close(parentheses, i);
            else if (tokens[i].text == "{")
                braces.push(i);
            else if (tokens[i].text == "}")
                close(braces, i);
            else if (tokens[i].text == "[")
                brackets.push(i);
            else if (tokens[i].text == "]")
                close(brackets, i);
        }
        return matching;
    }

    std::vector<FunctionRange> findFunctions(const std::vector<Token> &tokens,
                                             const std::vector<int> &matching)
    {
        std::vector<FunctionRange> functions;
        for (std::size_t i = 0; i + 2 < tokens.size(); ++i)
        {
            if (!isIdentifier(tokens[i]) || tokens[i + 1].text != "(")
                continue;
            const int closeParenthesis = matching[i + 1];
            if (closeParenthesis < 0)
                continue;
            const std::size_t next = closeParenthesis + 1;
            if (next >= tokens.size() || tokens[next].text != "{" ||
                matching[next] < 0)
                continue;
            functions.push_back({tokens[i].text, next + 1, (std::size_t)matching[next]});
            i = next;
        }
        return functions;
    }

    std::size_t statementEnd(const std::vector<Token> &tokens,
                             const std::vector<int> &matching,
                             std::size_t start, std::size_t limit)
    {
        if (start >= limit)
            return start;
        if (tokens[start].text == "{" && matching[start] >= 0)
        {
            return matching[start];
        }
        if (
            (tokens[start].text == "for" || tokens[start].text == "while" || tokens[start].text == "if") &&
            start + 1 < limit &&
            tokens[start + 1].text == "(" && matching[start + 1] >= 0
        )
        {
            return statementEnd(tokens, matching,
                                static_cast<std::size_t>(matching[start + 1]) + 1,
                                limit);
        }
        for (std::size_t i = start; i < limit; ++i)
        {
            if (tokens[i].text == ";")
                return i;
        }
        return limit - 1;
    }

    Growth classifyFor(const std::vector<Token> &tokens, std::size_t begin,
                       std::size_t end)
    {
        if (contains(tokens, begin, end, ":") && !contains(tokens, begin, end, ";"))
        {
            return Growth::Linear;
        }
        if (contains(tokens, begin, end, "*=") ||
            contains(tokens, begin, end, "/=") ||
            contains(tokens, begin, end, "<<=") ||
            contains(tokens, begin, end, ">>="))
            return Growth::Logarithmic;

        bool variableLimit = false;
        bool numberLimit = false;
        for (std::size_t i = begin; i + 1 < end; ++i)
        {
            if (tokens[i].text == "<" || tokens[i].text == "<=" ||
                tokens[i].text == ">" || tokens[i].text == ">=")
            {
                variableLimit = variableLimit || isIdentifier(tokens[i + 1]);
                numberLimit = numberLimit || isNumber(tokens[i + 1]);
            }
        }
        if (variableLimit || contains(tokens, begin, end, "size"))
            return Growth::Linear;
        if (numberLimit)
            return Growth::Constant;
        return Growth::Linear;
    }

    Growth classifyWhile(const std::vector<Token> &tokens,
                         std::size_t bodyBegin, std::size_t bodyEnd)
    {
        if (contains(tokens, bodyBegin, bodyEnd, "*=") ||
            contains(tokens, bodyBegin, bodyEnd, "/=") ||
            contains(tokens, bodyBegin, bodyEnd, "<<=") ||
            contains(tokens, bodyBegin, bodyEnd, ">>="))
            return Growth::Logarithmic;

        const bool calculatesHalf = contains(tokens, bodyBegin, bodyEnd, "/") &&
                                    contains(tokens, bodyBegin, bodyEnd, "2");
        const bool shrinksRange = contains(tokens, bodyBegin, bodyEnd, "+") ||
                                  contains(tokens, bodyBegin, bodyEnd, "-");
        if (calculatesHalf && shrinksRange)
            return Growth::Logarithmic;

        if (contains(tokens, bodyBegin, bodyEnd, "++") ||
            contains(tokens, bodyBegin, bodyEnd, "--") ||
            contains(tokens, bodyBegin, bodyEnd, "+=") ||
            contains(tokens, bodyBegin, bodyEnd, "-=") ||
            contains(tokens, bodyBegin, bodyEnd, "pop"))
            return Growth::Linear;
        return Growth::Unknown;
    }

    std::vector<LoopInfo> findLoops(const std::vector<Token> &tokens,
                                    const std::vector<int> &matching,
                                    std::size_t begin, std::size_t end)
    {
        std::vector<LoopInfo> loops;
        for (std::size_t i = begin; i + 1 < end; ++i)
        {
            if (tokens[i].text != "for" && tokens[i].text != "while")
                continue;
            if (tokens[i + 1].text != "(" || matching[i + 1] < 0)
                continue;
            const std::size_t headerEnd = matching[i + 1];
            const std::size_t bodyStart = headerEnd + 1;
            if (bodyStart >= end)
                continue;
            const std::size_t bodyEnd = statementEnd(tokens, matching, bodyStart, end);
            const Growth growth = tokens[i].text == "for"
                                      ? classifyFor(tokens, i + 2, headerEnd)
                                      : classifyWhile(tokens, bodyStart, bodyEnd + 1);
            loops.push_back({tokens[i].text, i, bodyStart, bodyEnd, growth});
        }
        return loops;
    }

    void includeGrowth(ComplexityValue &value, Growth growth)
    {
        if (growth == Growth::Linear)
            ++value.linearPower;
        else if (growth == Growth::Logarithmic)
            ++value.logPower;
        else if (growth == Growth::Unknown)
            value.unknown = true;
    }

    int valueScore(const ComplexityValue &value)
    {
        if (value.unknown)
            return 5;
        return value.linearPower * 20 + value.logPower * 5;
    }

    ComplexityValue largestLoopCost(const std::vector<LoopInfo> &loops)
    {
        ComplexityValue largest;
        for (const LoopInfo &loop : loops)
        {
            ComplexityValue current;
            includeGrowth(current, loop.growth);
            for (const LoopInfo &parent : loops)
            {
                if (parent.start < loop.start && parent.bodyStart <= loop.start &&
                    parent.bodyEnd >= loop.bodyEnd)
                    includeGrowth(current, parent.growth);
            }
            if (valueScore(current) > valueScore(largest))
                largest = current;
        }
        return largest;
    }

    std::string loopComplexity(const ComplexityValue &value)
    {
        return ComplexityTree::fromPowers(value.linearPower, value.logPower,
                                          value.unknown)
            .toBigO();
    }

    std::vector<std::size_t> selfCalls(const std::vector<Token> &tokens,
                                       const FunctionRange &function)
    {
        std::vector<std::size_t> calls;
        for (std::size_t i = function.bodyStart; i + 1 < function.bodyEnd; ++i)
        {
            if (tokens[i].text == function.name && tokens[i + 1].text == "(")
            {
                calls.push_back(i);
            }
        }
        return calls;
    }

    bool argumentContains(const std::vector<Token> &tokens,
                          const std::vector<int> &matching,
                          std::size_t call, const std::string &text)
    {
        if (call + 1 >= tokens.size() || matching[call + 1] < 0)
            return false;
        return contains(tokens, call + 2,
                        static_cast<std::size_t>(matching[call + 1]), text);
    }

    bool callInsideLoop(const std::vector<std::size_t> &calls,
                        const std::vector<LoopInfo> &loops)
    {
        for (std::size_t call : calls)
        {
            for (const LoopInfo &loop : loops)
            {
                if (call >= loop.bodyStart && call <= loop.bodyEnd)
                    return true;
            }
        }
        return false;
    }

    bool usesLinearSpace(const std::vector<Token> &tokens,
                         std::size_t begin, std::size_t end)
    {
        for (std::size_t i = begin; i + 2 < end; ++i)
        {
            if (tokens[i].text == "new" &&
                contains(tokens, i + 1, std::min(i + 10, end), "["))
                return true;
            const bool container = tokens[i].text == "vector" ||
                                   tokens[i].text == "list" ||
                                   tokens[i].text == "deque" ||
                                   tokens[i].text == "queue" ||
                                   tokens[i].text == "stack";
            if (container)
                return true;
        }
        return false;
    }

    void  detectStructures(const std::vector<Token> &tokens, AnalysisResult &result)
    {
        const std::vector<std::pair<std::string, std::string>> types = {
            {"vector", "Vector"}, {"list", "Linked List"}, {"forward_list", "Linked List"}, {"stack", "Stack"}, {"queue", "Queue"}, {"deque", "Deque"}, {"priority_queue", "Heap"}, {"unordered_map", "Hash Table"}, {"unordered_set", "Hash Table"}, {"map", "Binary Search Tree"}, {"set", "Binary Search Tree"}};
        for (const Token &token : tokens)
        {
            for (const auto &[codeName, displayName] : types)
            {
                if (token.text == codeName)
                    addOnce(result.detectedStructures, displayName);
            }
            if (token.text == "[")
                addOnce(result.detectedStructures, "Array / Indexing");
        }
        const bool customTree = contains(tokens, 0, tokens.size(), "left") &&
                                contains(tokens, 0, tokens.size(), "right") &&
                                (contains(tokens, 0, tokens.size(), "struct") ||
                                 contains(tokens, 0, tokens.size(), "class"));
        if (customTree)
            addOnce(result.detectedStructures, "Binary Search Tree");
    }

    CallGraph buildCallGraph(const std::vector<Token> &tokens, const std::vector<FunctionRange> &functions)
    {
        CallGraph graph;
        for (const FunctionRange &function : functions)
            graph.addFunction(function.name);
        for (const FunctionRange &function : functions)
        {
            const int from = graph.indexOf(function.name);
            for (std::size_t i = function.bodyStart; i + 1 < function.bodyEnd; ++i)
            {
                if (!isIdentifier(tokens[i]) || tokens[i + 1].text != "(")
                    continue;
                const int to = graph.indexOf(tokens[i].text);
                if (to >= 0)
                    graph.addCall(from, to);
            }
        }
        return graph;
    }

    bool looksLikeTraversal(const std::vector<Token> &tokens,
                            const FunctionRange &function,
                            const std::vector<LoopInfo> &loops,
                            const std::vector<std::size_t> &recursiveCalls)
    {
        const bool visited = contains(tokens, function.bodyStart, function.bodyEnd, "visited");
        const bool worklist = (contains(tokens, function.bodyStart, function.bodyEnd, "queue") ||
                               contains(tokens, function.bodyStart, function.bodyEnd, "stack")) &&
                              contains(tokens, function.bodyStart, function.bodyEnd, "pop");
        const bool nestedIteration = loops.size() >= 2;
        const bool recursiveIteration = !recursiveCalls.empty() && !loops.empty();
        return visited && ((worklist && nestedIteration) || recursiveIteration);
    }

    FunctionReport analyzeFunction(const std::vector<Token> &tokens,
                                   const std::vector<int> &matching,
                                   const FunctionRange &function,
                                   const std::vector<FunctionRange> &functions,
                                   const CallGraph &graph)
    {
        const std::vector<LoopInfo> loops = findLoops(tokens, matching,
                                                      function.bodyStart,
                                                      function.bodyEnd);
        const ComplexityValue local = largestLoopCost(loops);
        const std::vector<std::size_t> calls = selfCalls(tokens, function);
        FunctionReport report;
        report.name = function.name;
        report.graphIndex = graph.indexOf(function.name);
        report.time = loopComplexity(local);
        report.space = usesLinearSpace(tokens, function.bodyStart, function.bodyEnd)
                           ? "O(n)"
                           : "O(1)";
        report.score = valueScore(local);

        const bool followsTreeLinks =
            contains(tokens, function.bodyStart, function.bodyEnd, "left") &&
            contains(tokens, function.bodyStart, function.bodyEnd, "right") &&
            contains(tokens, function.bodyStart, function.bodyEnd, "->");

        if (looksLikeTraversal(tokens, function, loops, calls))
        {
            report.time = "O(V + E)";
            report.space = "O(V)";
            report.score = 45;
            report.reason = "visited graph traversal";
            return report;
        }

        if (calls.empty())
        {
            if (followsTreeLinks && !loops.empty())
            {
                report.time = "O(h), O(log n) balanced, O(n) worst";
                report.space = "O(1)";
                report.reason = "iterative binary-tree descent";
                report.score = 25;
                return report;
            }
            if (graph.hasCycleFrom(report.graphIndex))
            {
                report.time = "O(?)";
                report.space = "O(?)";
                report.reason = "indirect recursion cycle found by DFS";
                report.score = 10;
            }
            return report;
        }

        bool allHalve = true;
        bool allDecrease = true;
        for (std::size_t call : calls)
        {
            allHalve = allHalve &&
                       (argumentContains(tokens, matching, call, "/") ||
                        argumentContains(tokens, matching, call, ">>"));
            allDecrease = allDecrease && argumentContains(tokens, matching, call, "-");
        }
        const bool computesMiddle = contains(tokens, function.bodyStart,
                                             function.bodyEnd, "/") &&
                                    contains(tokens, function.bodyStart,
                                             function.bodyEnd, "2");
        if (calls.size() >= 2 && computesMiddle)
            allHalve = true;

        const bool recursiveInsideLoop = callInsideLoop(calls, loops);
        bool linearHelperWork = false;
        bool linearHelperSpace = false;
        for (std::size_t i = function.bodyStart; i + 1 < function.bodyEnd; ++i)
        {
            if (!isIdentifier(tokens[i]) || tokens[i + 1].text != "(" ||
                tokens[i].text == function.name)
                continue;
            const auto helper = std::find_if(
                functions.begin(), functions.end(),
                [&tokens, i](const FunctionRange &candidate)
                {
                    return candidate.name == tokens[i].text;
                });
            if (helper != functions.end())
            {
                const ComplexityValue helperWork = largestLoopCost(
                    findLoops(tokens, matching, helper->bodyStart, helper->bodyEnd));
                linearHelperWork = linearHelperWork || helperWork.linearPower >= 1;
                linearHelperSpace = linearHelperSpace ||
                                    usesLinearSpace(tokens, helper->bodyStart, helper->bodyEnd);
            }
        }
        const bool linearLocal = local.linearPower >= 1 || linearHelperWork;

        if (followsTreeLinks)
        {
            report.time = "O(h), O(log n) balanced, O(n) worst";
            report.space = "O(h), O(log n) balanced, O(n) worst";
            report.score = 30;
            report.reason = "recursive binary-tree descent";
        }
        else if (calls.size() >= 2 && !allHalve && linearHelperWork)
        {
            report.time = "O(n log n) average, O(n^2) worst";
            report.space = "O(log n) average, O(n) worst";
            report.score = 50;
            report.reason = "two-way partition recurrence";
        }
        else if (recursiveInsideLoop && allDecrease)
        {
            report.time = "O(n!)";
            report.space = "O(n)";
            report.score = 100;
            report.reason = "T(n) = nT(n-1) + local work";
        }
        else if (calls.size() >= 2 && allDecrease)
        {
            report.time = "O(2^n)";
            report.space = "O(n)";
            report.score = 90;
            report.reason = "branching T(n) = 2T(n-1) + local work";
        }
        else if (calls.size() >= 2 && allHalve)
        {
            report.time = linearLocal ? "O(n log n)" : "O(n)";
            report.space = linearHelperSpace ? "O(n)" : "O(log n)";
            report.score = linearLocal ? 50 : 35;
            report.reason = linearLocal ? "T(n) = 2T(n/2) + O(n)"
                                        : "T(n) = 2T(n/2) + O(1)";
        }
        else if (calls.size() == 1 && allHalve)
        {
            report.time = linearLocal ? "O(n)" : "O(log n)";
            report.space = "O(log n)";
            report.score = linearLocal ? 30 : 15;
            report.reason = linearLocal ? "T(n) = T(n/2) + O(n)"
                                        : "T(n) = T(n/2) + O(1)";
        }
        else if (calls.size() == 1 && allDecrease)
        {
            report.time = linearLocal ? "O(n^2)" : "O(n)";
            report.space = "O(n)";
            report.score = linearLocal ? 40 : 20;
            report.reason = linearLocal ? "T(n) = T(n-1) + O(n)"
                                        : "T(n) = T(n-1) + O(1)";
        }
        else
        {
            report.time = "O(?)";
            report.space = "O(?)";
            report.score = 10;
            report.reason = "unsupported recurrence";
        }
        return report;
    }

    AnalysisResult ComplexityAnalyzer::analyze(const std::string &sourceCode) const
    {
        AnalysisResult result;
        const dsa::LinkedList<Token> tokenList = Tokenizer().tokenize(sourceCode);
        const std::vector<Token> tokens(tokenList.begin(), tokenList.end());
        const std::vector<int> matching = buildMatchingPairs(tokens);
        const std::vector<FunctionRange> functions = findFunctions(tokens, matching);
        detectStructures(tokens, result);

        const CallGraph graph = buildCallGraph(tokens, functions);
        std::vector<FunctionReport> reports;
        for (const FunctionRange &function : functions)
        {
            reports.push_back(analyzeFunction(tokens, matching, function, functions, graph));
        }

        dsa::mergeSort(reports, [](const FunctionReport &left,
                                   const FunctionReport &right)
                       { return left.score > right.score; });

        const int algorithmIndex = graph.indexOf("algorithm");
        if (algorithmIndex < 0)
        {
            result.timeComplexity = "O(?)";
            result.spaceComplexity = "O(?)";
            return result;
        }

        const std::vector<int> reachable = graph.reachableByBfs(algorithmIndex);
        const auto isReachable = [&reachable](int index)
        {
            return std::find(reachable.begin(), reachable.end(), index) != reachable.end();
        };

        const FunctionReport *dominant = nullptr;
        for (const FunctionReport &report : reports)
        {
            if (!isReachable(report.graphIndex))
                continue;
            const std::vector<int> path = graph.shortestPath(algorithmIndex, report.graphIndex);
            if (!path.empty())
            {
                dominant = &report;
                break;
            }
        }

        if (dominant == nullptr)
        {
            result.timeComplexity = "O(?)";
            result.spaceComplexity = "O(?)";
        }
        else
        {
            result.timeComplexity = dominant->time;
            result.spaceComplexity = dominant->space;
        }
        return result;
    }

} // namespace codeforge
