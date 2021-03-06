#ifndef RDB_PROTOCOL_OP_HPP_
#define RDB_PROTOCOL_OP_HPP_

#include <algorithm>
#include <initializer_list>
#include <map>
#include <string>
#include <set>
#include <vector>

#include "rdb_protocol/env.hpp"
#include "rdb_protocol/error.hpp"
#include "rdb_protocol/term.hpp"
#include "rdb_protocol/val.hpp"

namespace ql {

// Specifies the range of normal arguments a function can take.
class argspec_t {
public:
    explicit argspec_t(int n);
    argspec_t(int _min, int _max);
    std::string print();
    bool contains(int n) const;
private:
    int min, max; // max may be -1 for unbounded
};

// Specifies the optional arguments a function can take.
struct optargspec_t {
public:
    explicit optargspec_t(std::initializer_list<const char *> args);

    static optargspec_t make_object();
    bool is_make_object() const;

    bool contains(const std::string &key) const;

    optargspec_t with(std::initializer_list<const char *> args) const;

private:
    void init(int num_args, const char *const *args);
    explicit optargspec_t(bool _is_make_object_val);
    bool is_make_object_val;

    std::set<std::string> legal_args;
};

// Almost all terms will inherit from this and use its member functions to
// access their arguments.
class op_term_t : public term_t {
public:
    op_term_t(env_t *env, protob_t<const Term> term,
              argspec_t argspec, optargspec_t optargspec = optargspec_t({}));
    virtual ~op_term_t();
protected:
    size_t num_args() const; // number of arguments
    counted_t<val_t> arg(size_t i, eval_flags_t flags = NO_FLAGS); // returns argument `i`
    // Tries to get an optional argument, returns `counted_t<val_t>()` if not
    // found.
    counted_t<val_t> optarg(const std::string &key);
    // This returns an optarg which is:
    // * lazy -- it's wrapped in a function, so you don't get the value until
    //   you call that function.
    // * literal -- it checks whether this operation has the literal key you
    //   provided and doesn't look anywhere else for optargs (in particular, it
    //   doesn't check global optargs).
    counted_t<func_t> lazy_literal_optarg(const std::string &key);
private:
    virtual bool is_deterministic_impl() const;
    std::vector<counted_t<term_t> > args;

    friend class make_obj_term_t; // needs special access to optargs
    std::map<std::string, counted_t<term_t> > optargs;
};

class bounded_op_term_t : public op_term_t {
public:
    bounded_op_term_t(env_t *env, protob_t<const Term> term,
                      argspec_t argspec, optargspec_t optargspec = optargspec_t({}))
        : op_term_t(env, term, argspec,
                    optargspec.with({"left_bound", "right_bound"})),
          left_open_(false), right_open_(true) {
        left_open_ = open_bool("left_bound", false);
        right_open_ = open_bool("right_bound", true);
    }
    virtual ~bounded_op_term_t() { }
protected:
    bool left_open() { return left_open_; }
    bool right_open() { return right_open_; }
private:
    bool open_bool(const std::string &key, bool def/*ault*/) {
        counted_t<val_t> v = optarg(key);
        if (!v.has()) return def;
        const std::string &s = v->as_str();
        if (s == "open") {
            return true;
        } else if (s == "closed") {
            return false;
        } else {
            rfail(base_exc_t::GENERIC,
                  "Expected `open` or `closed` for optarg `%s` (got `%s`).",
                  key.c_str(), v->trunc_print().c_str());
        }
    }
    bool left_open_, right_open_;
};

}  // namespace ql

#endif // RDB_PROTOCOL_OP_HPP_
