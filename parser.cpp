#include <assert.h>
#include <stdio.h>
#include <fstream>
#include <iostream>
#include <stack>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#define EMPTY "#"  // 将空串定义为# 因为产生式中用的是#表示空
using namespace std;

struct Token {
    string val;
    string label;
};

struct Node {
    Token token;
    vector<Node*> children;
};

class Parser {
   private:
    string fileName_productions, fileName_lex, out_path;
    unordered_set<string> productions;                               // 产生式集合
    unordered_map<string, unordered_set<string>> split_productions;  // 分解后的产生式集合
    unordered_map<string, vector<string>> symbols;                   // 分解每个产生式右部的符号
    unordered_set<string> Vt;                                        // 终结符集合
    unordered_set<string> Vn;                                        // 非终结符集合
    unordered_map<string, unordered_set<string>> First;              // First集
    unordered_map<string, unordered_set<string>> Follow;             // Follow集
    unordered_map<string, unordered_map<string, string>> table;      // 预测分析表
    vector<Token> tokens;
    Node* root = new Node;

    void readProductions();   // 从文件读取产生式
    void splitProductions();  // 分解产生式
    void getSymbols();        // 分解产生式的右部得到单独符号
    void findVnVt();          // 获得非终结符和终结符
    bool isVn(string);
    bool isVt(string);
    void getFirst(string, unordered_set<string>&);                  // 获得一个非终结符的first集
    void getRightFirst(string, unordered_set<string>&);             // 取得一个产生式右部的first集
    void getAllFirst();                                             // 获得所有非终结符first集
    void getAllFollow();                                            // 获得所有非终结符follow集
    void repeat();                                                  // 求follow集的重复过程 因为有follow集之间的依赖
    int mergeFollow(string A, string B);                            // 将的follow(B)加入follow(A) 返回follow(A)增加的数量
    bool isLL1();                                                   // 判断是否是LL1文法
    bool isDisjoint(unordered_set<string>, unordered_set<string>);  // 判断两集合是否相交
    void getTable();                                                // 构造预测分析表
    void readTokens();                                              // 获取词法分析器结果
    void buildAST();                                                // 构建抽象语法树
    void writeAST();                                                // 将抽象语法树写入输出文件
    void dfs_write(Node*, int, ofstream&);                          // dfs树并输出

   public:
    Parser(string p = "productions.txt", string lex = "lex_result.txt", string out = "parser_result.txt")
        : fileName_productions(p), fileName_lex(lex), out_path(out) {}
    void run();
};

void Parser::buildAST() {
    stack<Node*> st_node;  // 树的节点栈
    Node* p = new Node;
    p->token.label = "$";
    st_node.push(p);
    st_node.push(root);
    int index = 0;  // 当前token位置
    while (!st_node.empty()) {
        Node* cur_node = st_node.top();
        string symbol = cur_node->token.label;
        Token token = tokens[index];
        // 匹配
        if (token.label == symbol) {
            cur_node->token.val = token.val;
            index++;
            st_node.pop();
            continue;
        }
        // symbol是非终结符 查预测分析表
        assert(isVn(symbol));
        if (table[symbol].find(token.label) == table[symbol].end()) {
            printf("预测分析表与输入不匹配");
            break;
        }
        string right = table[symbol][token.label];
        st_node.pop();
        if (right != "#") {
            for (int i = 0; i < symbols[right].size(); i++) {
                Node* child = new Node;
                child->token.label = symbols[right][i];
                cur_node->children.push_back(child);
            }
            for (int i = cur_node->children.size() - 1; i >= 0; i--) {
                st_node.push(cur_node->children[i]);
            }
        }
    }
}

void Parser::writeAST() {
    ofstream out;
    out.open(out_path, ios::out);
    int depth = 0;
    Node* p = root;
    dfs_write(p, depth, out);
}

void Parser::dfs_write(Node* cur, int depth, ofstream& out) {
    for (int i = 0; i < depth; i++) {
        out << "  ";
    }
    out << cur->token.label;
    if (cur->token.label == "ID" || cur->token.label == "NUM") {
        out << ":" << cur->token.val;
    }
    out << endl;
    for (auto child : cur->children) {
        dfs_write(child, depth + 1, out);
    }
}

void Parser::readTokens() {
    string line;
    ifstream fin(fileName_lex);
    assert(fin.is_open());
    while (getline(fin, line)) {
        // 跳过注释
        if (line.find("ANNOTATION") != string::npos)
            continue;
        string temp[4];
        int cur = 0;
        int pos = line.find("\t", cur);  // 各个符号是由空格分割的
        for (int i = 0; i < 4 && pos != string::npos; i++) {
            temp[i] = line.substr(cur, pos - cur);
            cur = pos + 1;
            pos = line.find("\t", cur);
        }
        temp[3] = line.substr(cur, pos - cur);
        Token token;
        token.val = temp[2];
        token.label = temp[3];
        tokens.push_back(token);
    }
    Token token;
    token.val = "$";
    token.label = "$";
    tokens.push_back(token);
}

void Parser::getTable() {
    for (auto it : split_productions) {
        string left = it.first;
        for (string right : it.second) {
            unordered_set<string> uset;  // right 的first集
            getRightFirst(right, uset);
            if (uset.find("#") != uset.end()) {
                for (string symbol : Follow[left]) {
                    assert(table[left].find(symbol) == table[left].end());  // 确保没有冲突
                    table[left][symbol] = right;
                }
                uset.erase("#");
            }
            for (string symbol : uset) {
                assert(table[left].find(symbol) == table[left].end());  // 确保没有冲突
                table[left][symbol] = right;
            }
        }
    }
}

void Parser::getRightFirst(string right, unordered_set<string>& uset) {
    for (string symbol : symbols[right]) {
        if (isVt(symbol) || symbol == "#") {
            uset.insert(symbol);
            break;
        }
        assert(isVn(symbol));
        uset.insert(First[symbol].begin(), First[symbol].end());
        if (uset.find("#") == uset.end())
            break;
        // 若其first集中有#且不是最后一个符号 则去掉#并继续看下一个symbol
        else if (symbol != symbols[right][symbols[right].size() - 1]) {
            uset.erase("#");
        }
    }
}

bool Parser::isDisjoint(unordered_set<string> A, unordered_set<string> B) {
    for (string a : A) {
        if (B.find(a) != B.end())
            return false;
    }
    return true;
}

bool Parser::isLL1() {
    int flag = 1;
    for (auto it : split_productions) {
        string left = it.first;
        auto right_set = it.second;
        if (right_set.size() == 1)
            continue;
        // 形如A->a|b 若a和b均不能推出#则first(a)与first(b)应无交集
        // a和b至多有一个能推出# 若a->#则first(b)与follow(A)无交集 若b->#则first(a)与follow(A)无交集
        vector<unordered_set<string>> temp;  // 存储a,b的first集
        for (string right : right_set) {
            unordered_set<string> uset;
            getRightFirst(right, uset);
            temp.push_back(uset);
        }
        // 比较temp集中各个集合是否满足LL1要求
        for (int i = 0; i < temp.size() - 1; i++) {
            for (int j = i + 1; j < temp.size(); j++) {
                if (temp[i].find("#") == temp[i].end() && temp[j].find("#") == temp[j].end()) {
                    if (!isDisjoint(temp[i], temp[j]))
                        flag = 0;
                } else if (temp[i].find("#") != temp[i].end() && temp[j].find("#") == temp[j].end()) {
                    if (!isDisjoint(Follow[left], temp[j]))
                        flag = 0;
                } else if (temp[i].find("#") == temp[i].end() && temp[j].find("#") != temp[j].end()) {
                    if (!isDisjoint(Follow[left], temp[i]))
                        flag = 0;
                } else
                    flag = 0;
            }
        }
    }
    if (flag == 0)
        return false;
    return true;
}

void Parser::getSymbols() {
    for (auto it : split_productions) {
        for (string right : it.second) {
            // 若已经求过该产生式右部的符号 则跳过
            if (symbols.find(right) != symbols.end())
                continue;
            int cur = 0;
            int pos = right.find(" ", cur);  // 各个符号是由空格分割的
            while (pos != string::npos) {
                symbols[right].push_back(right.substr(cur, pos - cur));
                cur = pos + 1;
                pos = right.find(" ", cur);
            }
            symbols[right].push_back(right.substr(cur));
        }
    }
}

void Parser::getFirst(string left, unordered_set<string>& oneFirst) {
    unordered_set<string> right_set = split_productions[left];
    for (string right : right_set) {
        // 若产生式右部为# 直接添加进first集
        if (right == "#") {
            // First[left].insert("#");
            oneFirst.insert("#");
            continue;
        }
        // 顺序遍历产生式右部符号 若为终结符则加入first集并停止遍历
        for (int i = 0; i < symbols[right].size(); i++) {
            // 若为终结符则加入first集并停止遍历
            if (isVt(symbols[right][i])) {
                oneFirst.insert(symbols[right][i]);
                break;
            }
            // 若为非终结符则找到其first集并添加进来
            assert(isVn(symbols[right][i]));  // 不是终结符一定是非终结符
            getFirst(symbols[right][i], First[symbols[right][i]]);
            // 若其first集中有# 则还要继续看下一个symbol
            unordered_set<string> temp = First[symbols[right][i]];
            auto it = temp.find("#");
            if (it != temp.end()) {
                // 若所有symbol的first集都有# 则也要给此first集加上#
                if (i != symbols[right].size() - 1) {
                    temp.erase(it);
                }
                oneFirst.insert(temp.begin(), temp.end());
            } else {
                oneFirst.insert(temp.begin(), temp.end());
                break;
            }
        }
    }
}

void Parser::getAllFirst() {
    for (auto it : split_productions) {
        getFirst(it.first, First[it.first]);
    }
}

void Parser::getAllFollow() {
    for (auto it : split_productions) {
        string left = it.first;
        for (string right : split_productions[left]) {
            int flag = 0;  // 判断是否将产生式左边符号的follow集加入右边某个符号
            for (int i = symbols[right].size() - 1; i >= 0; i--) {
                // 若此符号是非终结符
                if (isVn(symbols[right][i])) {
                    // 形如A->.....B 将followA加入followB
                    if (i == symbols[right].size() - 1 || flag == 1) {
                        Follow[symbols[right][i]].insert(Follow[left].begin(), Follow[left].end());
                        // 如果有B-># 则还要看是否将followA加入B前面符号的follow
                        if (split_productions[symbols[right][i]].find("#") != split_productions[symbols[right][i]].end()) {
                            flag = 1;
                        } else
                            flag = 0;
                    }
                    // 若i-1也为非终结符 将firsti 加入followi-1
                    if (i > 0 && isVn(symbols[right][i - 1])) {
                        Follow[symbols[right][i - 1]].insert(First[symbols[right][i]].begin(), First[symbols[right][i]].end());
                        Follow[symbols[right][i - 1]].erase("#");
                    }
                } else if (isVt(symbols[right][i]) && i > 0) {
                    flag = 0;
                    if (isVn(symbols[right][i - 1]))
                        Follow[symbols[right][i - 1]].insert(symbols[right][i]);
                }
            }
        }
    }
    repeat();
    // 消除if else的悬挂二义性
    Follow["selection-stmt"].erase("else");
    Follow["sel'"].erase("else");
}

void Parser::repeat() {
    int repeat_flag = 0;  // 结束迭代标志
    do {
        repeat_flag = 0;
        for (auto it : split_productions) {
            string left = it.first;
            for (string right : split_productions[left]) {
                int flag = 0;  // 判断是否将产生式左边符号的follow集加入右边某个符号
                for (int i = symbols[right].size() - 1; i >= 0; i--) {
                    // 若此符号是非终结符
                    if (isVn(symbols[right][i])) {
                        // 形如A->.....B 将followA加入followB
                        if (i == symbols[right].size() - 1 || flag == 1) {
                            // Follow[symbols[right][i]].insert(Follow[left].begin(), Follow[left].end());
                            int change = mergeFollow(symbols[right][i], left);
                            if (change > 0)
                                repeat_flag = 1;
                            // 如果有B-># 则还要看是否将followA加入B前面符号的follow
                            if (split_productions[symbols[right][i]].find("#") != split_productions[symbols[right][i]].end()) {
                                flag = 1;
                            } else
                                break;
                        }
                    } else if (isVt(symbols[right][i]) && i > 0) {
                        break;
                    }
                }
            }
        }
    } while (repeat_flag == 1);
}

int Parser::mergeFollow(string A, string B) {
    int cnt = 0;
    for (string symbol : Follow[B]) {
        if (Follow[A].find(symbol) == Follow[A].end()) {
            cnt++;
            Follow[A].insert(symbol);
        }
    }
    return cnt;
}

void Parser::readProductions() {
    string line;
    ifstream fin(fileName_productions);
    assert(fin.is_open());
    // 文法开始符号的follow集中放入$
    getline(fin, line);
    productions.insert(line);
    int pos = line.find("->");
    Follow[line.substr(0, pos)].insert("$");
    // 文法开始符号作为语法树根节点
    root->token.label = line.substr(0, pos);
    while (getline(fin, line)) {
        productions.insert(line);
    }
}

void Parser::splitProductions() {
    for (auto it = productions.begin(); it != productions.end(); it++) {
        string str = *it;
        int pos = str.find("->");
        assert(pos != string::npos);
        // 产生式左边右边的符号
        string left = str.substr(0, pos);
        string right = str.substr(pos + 2);
        if (split_productions.find(left) == split_productions.end()) {
            unordered_set<string> right_set;
            right_set.insert(right);
            split_productions[left] = right_set;
        } else {
            split_productions[left].insert(right);
        }
    }
}

void Parser::findVnVt() {
    // 产生式的左部是非终结符
    for (auto it = split_productions.begin(); it != split_productions.end(); it++) {
        Vn.insert(it->first);
    }
    // 在产生式右部且不是非终结符的是终结符
    for (auto it = split_productions.begin(); it != split_productions.end(); it++) {
        // 遍历每个产生式的右部
        for (auto right_p = it->second.begin(); right_p != it->second.end(); right_p++) {
            string right = *right_p;
            for (auto symbol : symbols[right]) {
                if (!isVn(symbol) && symbol != EMPTY) {
                    Vt.insert(symbol);
                }
            }
        }
    }
}

bool Parser::isVn(string s) {
    if (Vn.find(s) != Vn.end())
        return true;
    return false;
}

bool Parser::isVt(string s) {
    if (Vt.find(s) != Vt.end())
        return true;
    return false;
}

void Parser::run() {
    readProductions();
    splitProductions();
    getSymbols();
    findVnVt();
    getAllFirst();
    getAllFollow();
    if (!isLL1()) {
        printf("文法不是LL(1)文法");
        return;
    }
    getTable();
    readTokens();
    buildAST();
    writeAST();
    printf("已成功输出语法树");
}

int main() {
    Parser* parser = new Parser();
    parser->run();
}