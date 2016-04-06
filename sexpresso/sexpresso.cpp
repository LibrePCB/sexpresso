// Author: Isak Andersson 2016 bitpuffin dot com

#ifndef SEXPRESSO_OPT_OUT_PIKESTYLE
#include <vector>
#include <string>
#include <cstdint>
#endif
#include "sexpresso.hpp"

#include <cctype>
#include <stack>
#include <algorithm>
#include <sstream>
#include <array>

namespace sexpresso {
	Sexp::Sexp() {
		this->kind = SexpValueKind::SEXP;
	}
	Sexp::Sexp(std::string const& strval) {
		this->kind = SexpValueKind::STRING;
		this->value.str = strval;
	}
	Sexp::Sexp(std::vector<Sexp> const& sexpval) {
		this->kind = SexpValueKind::SEXP;
		this->value.sexp = sexpval;
	}

	auto Sexp::addChild(Sexp sexp) -> void {
		if(this->kind == SexpValueKind::STRING) {
			this->kind = SexpValueKind::SEXP;
			this->value.sexp.push_back(Sexp{std::move(this->value.str)});
		}
		this->value.sexp.push_back(std::move(sexp));
	}

	auto Sexp::addChild(std::string str) -> void {
		this->addChild(Sexp{std::move(str)});
	}

	auto Sexp::childCount() const -> size_t {
		switch(this->kind) {
		case SexpValueKind::SEXP:
			return this->value.sexp.size();
		case SexpValueKind::STRING:
			return 1;
		}
	}

	auto Sexp::getChildByPath(std::string const& path) -> Sexp* {
		if(this->kind == SexpValueKind::STRING) return nullptr;

		auto paths = std::vector<std::string>{};
		{
			auto start = path.begin();
			for(auto i = path.begin()+1; i != path.end(); ++i) {
				if(*i == '/') {
					paths.push_back(std::string{start, i});
					start = i + 1;
				}
			}
			paths.push_back(std::string{start, path.end()});
		}

		auto* cur = this;
		for(auto i = paths.begin(); i != paths.end();) {
			auto start = i;
			for(auto& child : cur->value.sexp) {
				auto brk = false;
				switch(child.kind) {
				case SexpValueKind::STRING:
					if(i == paths.end() - 1 && child.value.str == *i) return &child;
					else continue;
				case SexpValueKind::SEXP:
					if(child.value.sexp.size() == 0) continue;
					auto& fst = child.value.sexp[0];
					switch(fst.kind) {
					case SexpValueKind::STRING:
						if(fst.value.str == *i) {
							cur = &child;
							++i;
							brk = true;
						}
						break;
					case SexpValueKind::SEXP: continue;
					}
				}
				if(brk) break;
			}
			if(i == start) return nullptr;
			if(i == paths.end()) return cur;
		}
		return nullptr;
	}

	auto Sexp::getChild(size_t idx) -> Sexp& {
		return this->value.sexp[idx];
	}

	auto Sexp::getString() -> std::string& {
		return this->value.str;
	}

	static auto stringValToString(std::string const& s) -> std::string {
		if(s.size() == 0) return std::string{"\"\""};
		if(std::find(s.begin(), s.end(), ' ') == s.end()) return s;
		else return ('"' + s + '"');
	}

 static auto toStringImpl(Sexp const& sexp, std::ostringstream& ostream) -> void {
		switch(sexp.kind) {
		case SexpValueKind::STRING:
			ostream << stringValToString(sexp.value.str);
			break;
		case SexpValueKind::SEXP:
			switch(sexp.value.sexp.size()) {
			case 0:
				ostream << "()";
				break;
			case 1:
				ostream << '(';
				toStringImpl(sexp.value.sexp[0], ostream);
				ostream <<  ')';
				break;
			default:
				ostream << '(';
				for(auto i = sexp.value.sexp.begin(); i != sexp.value.sexp.end(); ++i) {
					toStringImpl(*i, ostream);
					if(i != sexp.value.sexp.end()-1) ostream << ' ';
				}
				ostream << ')';
			}
		}
	}

	auto Sexp::toString() const -> std::string {
		auto ostream = std::ostringstream{};
		// outer sexp does not get surrounded by ()
		switch(this->kind) {
		case SexpValueKind::STRING:
			ostream << stringValToString(this->value.str);
			break;
		case SexpValueKind::SEXP:
			for(auto i = this->value.sexp.begin(); i != this->value.sexp.end(); ++i) {
				toStringImpl(*i, ostream);
				if(i != this->value.sexp.end()-1) ostream << ' ';
			}
		}
		return ostream.str();
	}

	auto Sexp::isString() const -> bool {
		return this->kind == SexpValueKind::STRING;
	}

	auto Sexp::isSexp() const -> bool {
		return this->kind == SexpValueKind::SEXP;
	}

	auto Sexp::isNil() const -> bool {
		return this->kind == SexpValueKind::SEXP && this->childCount() == 0;
	}

	static auto childrenEqual(std::vector<Sexp> const& a, std::vector<Sexp> const& b) -> bool {
		if(a.size() != b.size()) return false;

		for(auto i = 0; i < a.size(); ++i) {
			if(!a[i].equal(b[i])) return false;
		}
		return true;
	}
	
	auto Sexp::equal(Sexp const& other) const -> bool {
		if(this->kind != other.kind) return false;
		switch(this->kind) {
		case SexpValueKind::SEXP:
			return childrenEqual(this->value.sexp, other.value.sexp);
			break;
		case SexpValueKind::STRING:
			return this->value.str == other.value.str;
		}
	}

	auto Sexp::arguments() -> SexpArgumentIterator {
		return SexpArgumentIterator{*this};
	}

	auto parse(std::string const& str, std::string& err) -> Sexp {
		static std::array<char, 11> escape_chars = { '\'', '"',  '?', '\\',  'a',  'b',  'f',  'n',  'r',  't',  'v' };
		static std::array<char, 11> escape_vals  = { '\'', '"', '\?', '\\', '\a', '\b', '\f', '\n', '\r', '\t', '\v' };

		auto sexprstack = std::stack<Sexp>{};
		sexprstack.push(Sexp{}); // root
		auto nextiter = str.begin();
		for(auto iter = nextiter; iter != str.end(); iter = nextiter) {
			nextiter = iter + 1;
			if(std::isspace(*iter)) continue;
			auto& cursexp = sexprstack.top();
			switch(*iter) {
			case '(':
				sexprstack.push(Sexp{});
				break;
			case ')': {
				auto topsexp = std::move(sexprstack.top());
				sexprstack.pop();
				if(sexprstack.size() == 0) {
					err = std::string{"too many ')' characters detected, closing sexprs that don't exist, no good."};
					return Sexp{};
				}
				auto& top = sexprstack.top();
				top.addChild(std::move(topsexp));
				break;
			}
			case '"': {
				// TODO: handle escape sequences
				auto i = iter+1;
				auto start = i;
				for(; i != str.end(); ++i) {
					if(*i == '\\') { ++i; continue; }
					if(*i == '"') break;
					if(*i == '\n') {
						err = std::string{"Unexpected newline in string literal"};
						return Sexp{};
					}
				}
				if(i == str.end()) {
					err = std::string{"Unterminated string literal"};
					return Sexp{};
				}
				auto resultstr = std::string{};
				resultstr.reserve(i - start);
				for(auto it = start; it != i; ++it) {
					switch(*it) {
					case '\\': {
						++it;
						if(it == i) {
							err = std::string{"Unfinished escape sequence at the end of the string"};
							return Sexp{};
						}
						auto pos = std::find(escape_chars.begin(), escape_chars.end(), *it);
						if(pos == escape_chars.end()) {
							err = std::string{"invalid escape char '"} + *it + '\'';
							return Sexp{};
						}
						resultstr.push_back(escape_vals[pos - escape_chars.begin()]);
						break;
					}
					default:
						resultstr.push_back(*it);
					}
				}
				sexprstack.top().addChild(Sexp{std::move(resultstr)});
				nextiter = i + 1;
				break;
			}
			case ';':
				for(; nextiter != str.end() && *nextiter != '\n' && *nextiter != '\r'; ++nextiter) {}
				for(; nextiter != str.end() && (*nextiter == '\n' || *nextiter == '\r'); ++nextiter) {}
				break;
			default:
				auto symend = std::find_if(iter, str.end(), [](char const& c) { return std::isspace(c) || c == ')'; });
				auto& top = sexprstack.top();
				top.addChild(Sexp{std::string{iter, symend}});
				nextiter = symend;
			}
		}
		if(sexprstack.size() != 1) {
			err = std::string{"not enough s-expressions were closed by the end of parsing"};
			return Sexp{};
		}
		return std::move(sexprstack.top());
	}

	auto parse(std::string const& str) -> Sexp {
		auto ignored_error = std::string{};
		return parse(str, ignored_error);
	}

	SexpArgumentIterator::SexpArgumentIterator(Sexp& sexp) : sexp(sexp) {}

	auto SexpArgumentIterator::begin() -> iterator {
		if(this->size() == 0) return this->end(); else return ++(this->sexp.value.sexp.begin());
	}

	auto SexpArgumentIterator::end() -> iterator { return this->sexp.value.sexp.end(); }

	auto SexpArgumentIterator::begin() const -> const_iterator {
		if(this->size() == 0) return this->end(); else return ++(this->sexp.value.sexp.begin());
	}

	auto SexpArgumentIterator::end() const -> const_iterator { return this->sexp.value.sexp.end(); }

	auto SexpArgumentIterator::empty() const -> bool { return this->size() == 0;}

	auto SexpArgumentIterator::size() const -> size_t {
		auto sz = this->sexp.value.sexp.size();
		if(sz == 0) return 0; else return sz-1;
	}
}
