#include <iostream>
#include <string>
#include <fstream> // for ifstream, ofstream
#include <sstream> // for istringstream (uneven spacing safe parsing)
using namespace std;

//* bucket_pos and chain_pos are used throughout for printing output messages
//* they are passed as references whose values we don't know during lookup/add/delete
//* upon lookup/add/delete they are initialised inside those functions which can then be used to print output messages

unsigned int SDBMHash(const string &str, unsigned int num_buckets)   
{
    unsigned int hash = 0;

    unsigned int len = str.length();

    for (unsigned int i = 0; i < len; i++)
    {
        hash = ((str[i]) + (hash << 6) + (hash << 16) - hash) % num_buckets; // to avoid overflow for long strings
        // a good hash because its not like naive ASCII sum
        // similar looking strings still hash to very different buckets
    }

    return hash;
}

class SymbolInfo // the actual info about a symbol within some fixed scope 
{
private:
    string name;
    string type;

public:
    SymbolInfo* next;

    SymbolInfo(const string &name, const string &type, SymbolInfo* next = nullptr)
    {
        this->name = name;
        this->type = type;
        this->next = next;
    }

    string getSymbolName() const
    {
        // the 'const' keyword after function defn prevents any modification of object variables inside this scope
        return name;
    }

    string getSymbolType() const
    {
        return type;
    }

    void setSymbolName(const string &name)
    { // string& passes a direct reference/pointer instead of copying the paramater into memory first
        // helps when huge strings are passed, no copy is needed and CPU cycles are not wasted
        // but since a reference is passed, one might risk modifiying what was passed to be written
        // const prevents that

        this->name = name;
    }

    void setSymbolType(const string &type)
    {
        this->type = type;
    }

    ~SymbolInfo()
    {
        delete next; // eventually, the entire chain will be deleted
    }
};

// ? Should I change index of bucket_pos to unsigned everywhere instead of converting types ?
// * YES — bucket_pos is now unsigned int throughout, matching SDBMHash's return type
// * chain_pos stays int because we use -1 as a sentinel for "not found" in lookUp
// * num_buckets is now unsigned int throughout, matching SDBMHash's parameter type

class ScopeTable // Simply stated, its just a hash-table for a particular scope
{
private:
    ScopeTable* parent_scope;
    SymbolInfo** buckets;
    int id;
    unsigned int num_buckets; // unsigned — a negative bucket count is meaningless, and matches SDBMHash's parameter type

public:
    ScopeTable(int id, unsigned int num_buckets, ScopeTable* parent_scope = nullptr)
    {
        this->id = id;
        this->num_buckets = num_buckets;
        this->parent_scope = parent_scope;

        this->buckets = new SymbolInfo *[num_buckets]; // array of pointers

        // initially each pointer points to some random parts of memory
        // so it needs cleaning because otherwise we would assume entire hash-table is already containing at least one key or a chain
        // and if we try to access that we get right into segmentation fault

        for (unsigned int i = 0; i < num_buckets; i++)
        {
            buckets[i] = nullptr;
        }
    }

    SymbolInfo* lookUp(const string &name) // looks for a symbol "name" within this scope-table
    {
        unsigned int bucket = SDBMHash(name, num_buckets); // found the index,where the symbol is stored within this hash-table, might be anywhere in this chain
        SymbolInfo* syminfo = buckets[bucket];

        while (syminfo != nullptr)
        {
            if (syminfo->getSymbolName() == name)
                return syminfo;
            else
                syminfo = syminfo->next;
        }

        return nullptr; // symbol is not present in this scope (could still be present in parent scopes)
    }

    // overload that also returns bucket position (0-indexed) and chain position (1-indexed from head)
    // needed for output messages like "Inserted in ScopeTable# 1 at position 3, 2"
    // bucket_pos is unsigned int — directly holds SDBMHash result with no cast needed
    // chain_pos is int — needs to hold -1 as a sentinel when the symbol is not found

    SymbolInfo* lookUp(const string &name, unsigned int &bucket_pos, int &chain_pos)
    {
        bucket_pos = SDBMHash(name, num_buckets); // no cast needed — both unsigned int now
        SymbolInfo* syminfo = buckets[bucket_pos];
        chain_pos = 1;

        while (syminfo != nullptr)
        {
            if (syminfo->getSymbolName() == name)
                return syminfo;
            else
            {
                syminfo = syminfo->next;
                chain_pos++;
            }
        }

        chain_pos = -1; // not found — int allows this sentinel value
        return nullptr;
    }

    bool insert(const string &name, const string &type, unsigned int &bucket_pos, int &chain_pos) 
    {
        SymbolInfo* syminfo = lookUp(name);
        if (syminfo != nullptr)
            return false; // symbol already present in this hash-table

        SymbolInfo* toInsert = new SymbolInfo(name, type);

        bucket_pos = SDBMHash(name, num_buckets);

        // insert at TAIL so that the oldest symbol stays at head
        if (buckets[bucket_pos] == nullptr)
        {
            buckets[bucket_pos] = toInsert; // bucket was empty, new node is the head
            chain_pos = 1;
        }
        else
        {
            // walk to the tail and append
            SymbolInfo* syminfo_curr = buckets[bucket_pos];
            int pos = 1;
            while (syminfo_curr->next != nullptr)
            {
                syminfo_curr = syminfo_curr->next;
                pos++;
            }
            syminfo_curr->next = toInsert;
            chain_pos = pos + 1; // new node is one past the old tail
        }

        return true;
    }

    bool remove(const string &name, unsigned int &bucket_pos, int &chain_pos) 
    {
        bucket_pos = SDBMHash(name, num_buckets); 
        SymbolInfo* syminfo = buckets[bucket_pos];
        SymbolInfo* syminfo_parent = nullptr;
        chain_pos = 1;

        while (syminfo != nullptr)
        {
            if (syminfo->getSymbolName() == name) // syminfo is the SymbolInfo we need to delete
            {
                if (syminfo_parent != nullptr) // deletion at mid/end
                {
                    syminfo_parent->next = syminfo->next;
                }
                else
                {
                    buckets[bucket_pos] = syminfo->next; // deletion at head
                }

                syminfo->next = nullptr; // completely detach syminfo to delete from the chain // ! otherwise, a cascading delete would delete everything after this symbol too
                delete syminfo;
                return true;
            }
            else
            {
                syminfo_parent = syminfo; // adjust parent pointer
                syminfo = syminfo->next;
                chain_pos++;
            }
        }

        return false; // failed to remove, because absent in this chain
    }

    // prints all buckets to the output file with the given tab indentation level
    // all buckets printed even if empty — confirmed by sample output
    // bucket numbers are 1-indexed in display, trailing space after each symbol
    void print(ofstream &out, int indent_level)
    {
        string indent(indent_level, '\t'); // * How many tabs to prepend before printing this scope table, could have used a for loop too.
        out << indent << "ScopeTable# " << id << "\n";

        for (unsigned int i = 0; i < num_buckets; i++)
        {
            out << indent << i + 1 << "--> "; // 1-indexed bucket display
            SymbolInfo* syminfo = buckets[i];
            while (syminfo != nullptr)
            {
                out << "<" << syminfo->getSymbolName() << "," << syminfo->getSymbolType() << "> "; // trailing space after each symbol
                syminfo = syminfo->next;
            }
            out << "\n";
        }
    }

    ScopeTable* getParentScope() const
    {
        return this->parent_scope;
    }

    void setParentScope(ScopeTable* scope)
    {
        this->parent_scope = scope;
    }

    int getScopeID() const
    {
        return this->id;
    }

    bool isRoot() const
    {
        return parent_scope == nullptr; // no parent means this is the global/root scope
    }

    ~ScopeTable()
    {
        for (unsigned int i = 0; i < num_buckets; i++)
        {
            delete buckets[i];
        }
        delete[] buckets;
    }
};


class SymbolTable // manages all the scope tables
{
private:
    ScopeTable* currentScope;
    int scope_id;
    unsigned int num_buckets; 

public:
    SymbolTable(unsigned int num_buckets, ofstream &out)
    {
        this->num_buckets = num_buckets;
        this->scope_id = 0;
        this->currentScope = nullptr; // * needed — enterScope passes currentScope as parent, must be nullptr for root

        enterScope(out);
    }

    SymbolInfo* lookUp(const string &name, int &found_scope_id, unsigned int &bucket_pos, int &chain_pos) // ? Maybe declare the int b,c outside the loop ?
    {
        ScopeTable* scope = currentScope; // temp variable to avoid changing the current scope during look up

        while (scope != nullptr)
        {
            unsigned int b; // unsigned — bucket index matches SDBMHash return type
            int c;          // signed — chain_pos needs -1 sentinel for not found
            SymbolInfo* syminfo = scope->lookUp(name, b, c);
            if (syminfo != nullptr)
            {
                found_scope_id = scope->getScopeID();
                bucket_pos = b;
                chain_pos = c;
                return syminfo;
            }
            else
                scope = scope->getParentScope();
        }

        return nullptr;
    }

    bool insert(const string &name, const string &type, unsigned int &bucket_pos, int &chain_pos) // adds to current scope
    {
        return currentScope->insert(name, type, bucket_pos, chain_pos);
    }

    bool remove(const string &name, unsigned int &bucket_pos, int &chain_pos) // removes from current scope
    {
        return currentScope->remove(name, bucket_pos, chain_pos);
    }

    void enterScope(ofstream &out)
    {
        scope_id++;
        ScopeTable* newScopeTable = new ScopeTable(scope_id, num_buckets, currentScope);
        currentScope = newScopeTable;
        out << "\tScopeTable# " << scope_id << " created\n";
    }

    // returns false if at root — caller treats the command as invalid and does not count it
    // ? Maybe add exitedScope -> parent_scope = nullptr too ?
    // * Not needed as there is no cascade to parents inside this object's destructor
    bool exitScope(ofstream &out)
    {
        if (currentScope->isRoot()) // this is the root scope, exit is forbidden
            return false;

        ScopeTable* exitedScope = currentScope;
        int exited_id = exitedScope->getScopeID();

        currentScope = currentScope->getParentScope(); // scope is now its parent
        delete exitedScope;

        out << "\tScopeTable# " << exited_id << " removed\n";
        return true;
    }

    void printCurrentScope(ofstream &out)
    {
        currentScope->print(out, 1);
    }

    void printAllScopes(ofstream &out)
    {
        ScopeTable* scope = currentScope;
        int indent = 1; // current scope gets 1 tab, each parent gets one more

        while (scope != nullptr)
        {
            scope->print(out, indent);
            scope = scope->getParentScope();
            indent++;
        }
    }

    int getCurrentScopeID() const
    {
        return currentScope->getScopeID();
    }

    bool currentIsRoot() const
    {
        return currentScope->isRoot();
    }

    // used by Q command — prints and destroys every scope from current down to root
    void destroyAll(ofstream &out)
    {
        while (currentScope != nullptr)
        {
            int exited_id = currentScope->getScopeID();
            ScopeTable* parent = currentScope->getParentScope();
            delete currentScope;
            currentScope = parent;
            out << "\tScopeTable# " << exited_id << " removed\n";
        }
    }

    ~SymbolTable()
    {
        // destructor — no output, just free remaining memory
        // destroyAll handles the Q path with printing, this handles any other exit
        while (currentScope != nullptr)
        {
            ScopeTable* parent = currentScope->getParentScope();
            delete currentScope;
            currentScope = parent;
        }

        /*while(currentScope -> getParentScope() != nullptr) // exit all scopes until global scope is reached
            exitScope();

        delete currentScope;
        currentScope = nullptr;*/
    }
};

// ! RESUME code reviewing HERE

// formats FUNCTION token list into the type string: FUNCTION,returntype<==(arg1,arg2,...)
// tokens[0] is return type, tokens[1..count-1] are argument types
string formatFunction(const string tokens[], int count)
{
    string full_type = "FUNCTION,";
    full_type += tokens[0];
    full_type += "<==(";
    for (int i = 1; i < count; i++)
    {
        if (i > 1)
            full_type += ",";
        full_type += tokens[i];
    }
    full_type += ")";
    return full_type;
}

// formats STRUCT/UNION token list into: STRUCT,{(type1,var1),(type2,var2),...}
// tokens come in pairs: type var type var ...
string formatStructUnion(const string &keyword, const string tokens[], int count)
{
    string full_type = keyword + ",{";
    for (int i = 0; i + 1 < count; i += 2)
    {
        if (i > 0)
            full_type += ",";
        full_type += "(" + tokens[i] + "," + tokens[i + 1] + ")";
    }
    full_type += "}";
    return full_type;
}

// trims trailing whitespace from a line
// needed because some input lines have trailing spaces that should not appear in the Cmd echo
string trimRight(const string &s)
{
    int end = (int)s.length() - 1;
    while (end >= 0 && (s[end] == ' ' || s[end] == '\t' || s[end] == '\r'))
        end--;
    return s.substr(0, end + 1);
}

int main(int argc, char* argv[])
{
    if (argc != 3)
    {
        cout << "Usage: " << argv[0] << " <input_file> <output_file>" << endl;
        return 1;
    }

    ifstream in(argv[1]);
    ofstream out(argv[2]);

    if (!in.is_open())
    {
        cout << "Cannot open input file\n";
        return 1;
    }
    if (!out.is_open())
    {
        cout << "Cannot open output file\n";
        return 1;
    }

    unsigned int num_buckets; // unsigned — matches SDBMHash and ScopeTable, negative bucket count is meaningless
    in >> num_buckets;
    in.ignore(); // consume the newline after the number

    SymbolTable table(num_buckets, out);

    string line;
    int cmd_count = 0; // counts only valid recognized commands for the "Cmd N:" prefix

    while (getline(in, line))
    {   
        //int* leak_test = new int[100];
        istringstream ss(line);
        string cmd;
        if (!(ss >> cmd))
            continue; // skip empty lines

        // validate the command before counting or printing it
        // invalid commands are silently ignored — no Cmd line, not counted
        // rules from sample output:
        //   unknown top-level commands (e.g. M) -> silently ignored
        //   P followed by anything other than A or C -> silently ignored
        //   E when already at root -> silently ignored, not counted

        if (cmd == "P")
        {
            string sub;
            if (!(ss >> sub) || (sub != "A" && sub != "C"))
                continue; // P X or bare P — silently ignored
        }
        else if (cmd == "E")
        {
            if (table.currentIsRoot())
                continue; // E on root — silently ignored, not counted
        }
        else if (cmd != "I" && cmd != "L" && cmd != "D" && cmd != "S" && cmd != "Q")
        {
            continue; // unknown command — silently ignored
        }

        // valid command — increment counter and echo the line
        cmd_count++;
        out << "Cmd " << cmd_count << ": " << trimRight(line) << "\n";

        // re-parse the line cleanly for execution
        istringstream ss2(line);
        ss2 >> cmd;

        if (cmd == "I")
        {
            string name, type_keyword;
            if (!(ss2 >> name >> type_keyword))
            {
                out << "\tNumber of parameters mismatch for the command I\n";
                continue;
            }

            // collect remaining tokens (no STL containers — fixed size array)
            string extra[64];
            int extra_count = 0;
            string tok;
            while (ss2 >> tok)
                extra[extra_count++] = tok;

            // build the formatted type string based on the type keyword
            string full_type;
            if (type_keyword == "FUNCTION")
                full_type = formatFunction(extra, extra_count);
            else if (type_keyword == "STRUCT" || type_keyword == "UNION")
                full_type = formatStructUnion(type_keyword, extra, extra_count);
            else
                full_type = type_keyword; // simple types like VAR, NUMBER, BOOL, RELOP etc.

            unsigned int bucket_pos; // unsigned — directly holds SDBMHash result
            int chain_pos;           // signed — chain positions are 1-indexed positive integers
            bool success = table.insert(name, full_type, bucket_pos, chain_pos);
            if (success)
                out << "\tInserted in ScopeTable# " << table.getCurrentScopeID()
                    << " at position " << bucket_pos + 1 // 1-indexed
                    << ", " << chain_pos << "\n";
            else
                out << "\t'" << name << "' already exists in the current ScopeTable\n";
        }
        else if (cmd == "L")
        {
            string name;
            if (!(ss2 >> name))
            {
                out << "\tNumber of parameters mismatch for the command L\n";
                continue;
            }
            // L takes exactly one parameter — extra tokens = mismatch
            string extra;
            if (ss2 >> extra)
            {
                out << "\tNumber of parameters mismatch for the command L\n";
                continue;
            }

            int found_scope_id;      // signed — scope IDs are stored as int
            unsigned int bucket_pos; // unsigned — directly holds SDBMHash result
            int chain_pos;           // signed — chain positions are 1-indexed positive integers
            SymbolInfo* syminfo = table.lookUp(name, found_scope_id, bucket_pos, chain_pos);
            if (syminfo != nullptr)
                out << "\t'" << name << "' found in ScopeTable# " << found_scope_id
                    << " at position " << bucket_pos + 1
                    << ", " << chain_pos << "\n";
            else
                out << "\t'" << name << "' not found in any of the ScopeTables\n";
        }
        else if (cmd == "D")
        {
            string name;
            if (!(ss2 >> name))
            {
                out << "\tNumber of parameters mismatch for the command D\n";
                continue;
            }

            unsigned int bucket_pos; // unsigned — directly holds SDBMHash result
            int chain_pos;           // signed — chain positions are 1-indexed positive integers
            bool success = table.remove(name, bucket_pos, chain_pos);
            if (success)
                out << "\tDeleted '" << name << "' from ScopeTable# " << table.getCurrentScopeID()
                    << " at position " << bucket_pos + 1
                    << ", " << chain_pos << "\n";
            else
                out << "\tNot found in the current ScopeTable\n";
        }
        else if (cmd == "P")
        {
            string sub;
            ss2 >> sub;
            if (sub == "A")
                table.printAllScopes(out);
            else
                table.printCurrentScope(out);
        }
        else if (cmd == "S")
        {
            table.enterScope(out);
        }
        else if (cmd == "E")
        {
            table.exitScope(out); // already validated above that this is not root
        }
        else if (cmd == "Q")
        {
            table.destroyAll(out); // print and destroy all remaining scopes
            break;                 // spec says no commands after Q are processed
        }
    }

    in.close();
    out.close();
    return 0;
}