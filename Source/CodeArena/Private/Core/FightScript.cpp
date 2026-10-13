#include "FightScript.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <unordered_set>

namespace FightScript
{
	static const size_t MaxListSize = 100000;
	static const size_t MaxStackSize = 100000;
	static const size_t MaxCallDepth = 200;

	// ------------------------------------------------------------------
	// Values
	// ------------------------------------------------------------------

	FValue FValue::MakeUndefined()
	{
		FValue V;
		V.Type = EValueType::Undefined;
		return V;
	}

	FValue FValue::MakeNone()
	{
		return FValue();
	}

	FValue FValue::MakeBool(bool bValue)
	{
		FValue V;
		V.Type = EValueType::Bool;
		V.BoolValue = bValue;
		return V;
	}

	FValue FValue::MakeNumber(double Value)
	{
		FValue V;
		V.Type = EValueType::Number;
		V.NumberValue = Value;
		return V;
	}

	FValue FValue::MakeString(const std::string& Value)
	{
		FValue V;
		V.Type = EValueType::String;
		V.StringValue = Value;
		return V;
	}

	FValue FValue::MakeList(std::vector<FValue> Items)
	{
		FValue V;
		V.Type = EValueType::List;
		V.ListValue = std::make_shared<std::vector<FValue>>(std::move(Items));
		return V;
	}

	FValue FValue::MakeFunction(int FunctionIndex)
	{
		FValue V;
		V.Type = EValueType::Function;
		V.Index = FunctionIndex;
		return V;
	}

	FValue FValue::MakeBuiltin(int BuiltinIndex)
	{
		FValue V;
		V.Type = EValueType::Builtin;
		V.Index = BuiltinIndex;
		return V;
	}

	bool FValue::IsTruthy() const
	{
		switch (Type)
		{
		case EValueType::Undefined:
		case EValueType::None: return false;
		case EValueType::Bool: return BoolValue;
		case EValueType::Number: return NumberValue != 0.0;
		case EValueType::String: return !StringValue.empty();
		case EValueType::List: return ListValue && !ListValue->empty();
		default: return true;
		}
	}

	bool FValue::Equals(const FValue& Other) const
	{
		// Python: True == 1, so compare bools and numbers numerically.
		const bool bNumericA = Type == EValueType::Number || Type == EValueType::Bool;
		const bool bNumericB = Other.Type == EValueType::Number || Other.Type == EValueType::Bool;
		if (bNumericA && bNumericB)
		{
			const double A = Type == EValueType::Bool ? (BoolValue ? 1.0 : 0.0) : NumberValue;
			const double B = Other.Type == EValueType::Bool ? (Other.BoolValue ? 1.0 : 0.0) : Other.NumberValue;
			return A == B;
		}
		if (Type != Other.Type)
		{
			return false;
		}
		switch (Type)
		{
		case EValueType::None: return true;
		case EValueType::String: return StringValue == Other.StringValue;
		case EValueType::List:
		{
			if (ListValue == Other.ListValue)
			{
				return true;
			}
			if (ListValue->size() != Other.ListValue->size())
			{
				return false;
			}
			for (size_t i = 0; i < ListValue->size(); ++i)
			{
				if (!(*ListValue)[i].Equals((*Other.ListValue)[i]))
				{
					return false;
				}
			}
			return true;
		}
		case EValueType::Function:
		case EValueType::Builtin: return Index == Other.Index;
		default: return false;
		}
	}

	std::string FormatNumber(double Value)
	{
		char Buffer[64];
		if (std::isfinite(Value) && std::fabs(Value) < 1e15 && Value == std::floor(Value))
		{
			std::snprintf(Buffer, sizeof(Buffer), "%lld", static_cast<long long>(Value));
		}
		else
		{
			std::snprintf(Buffer, sizeof(Buffer), "%.10g", Value);
		}
		return Buffer;
	}

	static std::string ReprValue(const FValue& Value)
	{
		if (Value.Type == EValueType::String)
		{
			return "'" + Value.StringValue + "'";
		}
		return Value.ToDisplayString();
	}

	std::string FValue::ToDisplayString() const
	{
		switch (Type)
		{
		case EValueType::Undefined: return "<undefined>";
		case EValueType::None: return "None";
		case EValueType::Bool: return BoolValue ? "True" : "False";
		case EValueType::Number: return FormatNumber(NumberValue);
		case EValueType::String: return StringValue;
		case EValueType::List:
		{
			std::string Out = "[";
			for (size_t i = 0; i < ListValue->size(); ++i)
			{
				if (i > 0)
				{
					Out += ", ";
				}
				Out += ReprValue((*ListValue)[i]);
				if (Out.size() > 2000)
				{
					Out += ", ...";
					break;
				}
			}
			return Out + "]";
		}
		case EValueType::Function: return "<function>";
		case EValueType::Builtin: return "<builtin function>";
		}
		return "?";
	}

	const char* FValue::TypeName() const
	{
		switch (Type)
		{
		case EValueType::Undefined: return "undefined";
		case EValueType::None: return "NoneType";
		case EValueType::Bool: return "bool";
		case EValueType::Number: return "number";
		case EValueType::String: return "str";
		case EValueType::List: return "list";
		case EValueType::Function: return "function";
		case EValueType::Builtin: return "builtin_function";
		}
		return "?";
	}

	// ------------------------------------------------------------------
	// Builtin table + standard library
	// ------------------------------------------------------------------

	int FBuiltinTable::AddFunction(const std::string& Name, int MinArgs, int MaxArgs, FBuiltinFn Fn)
	{
		FBuiltin B;
		B.Name = Name;
		B.MinArgs = MinArgs;
		B.MaxArgs = MaxArgs;
		B.Fn = std::move(Fn);
		const int NewIndex = static_cast<int>(Items.size());
		Items.push_back(std::move(B));
		ByName[Name] = NewIndex;
		return NewIndex;
	}

	int FBuiltinTable::AddConstant(const std::string& Name, const FValue& Value)
	{
		FBuiltin B;
		B.Name = Name;
		B.Constant = Value;
		const int NewIndex = static_cast<int>(Items.size());
		Items.push_back(std::move(B));
		ByName[Name] = NewIndex;
		return NewIndex;
	}

	int FBuiltinTable::Find(const std::string& Name) const
	{
		const auto It = ByName.find(Name);
		return It == ByName.end() ? -1 : It->second;
	}

	void FBuiltinTable::SetLocked(const std::string& Name, bool bLocked, const std::string& Message)
	{
		const int Found = Find(Name);
		if (Found >= 0)
		{
			Items[static_cast<size_t>(Found)].bLocked = bLocked;
			Items[static_cast<size_t>(Found)].LockedMessage = Message;
		}
	}

	static uint32_t NextRandom(uint32_t* State)
	{
		// xorshift32
		uint32_t X = *State != 0 ? *State : 0x9E3779B9u;
		X ^= X << 13;
		X ^= X >> 17;
		X ^= X << 5;
		*State = X;
		return X;
	}

	static bool ArgToInt(const FValue& Value, const char* FuncName, long long& Out, std::string& OutError)
	{
		if (Value.Type == EValueType::Bool)
		{
			Out = Value.BoolValue ? 1 : 0;
			return true;
		}
		if (Value.Type != EValueType::Number || Value.NumberValue != std::floor(Value.NumberValue))
		{
			OutError = std::string("TypeError: ") + FuncName + "() expects whole numbers, got " + Value.TypeName();
			return false;
		}
		Out = static_cast<long long>(Value.NumberValue);
		return true;
	}

	static bool MinMaxImpl(const std::vector<FValue>& Args, bool bMax, FValue& OutResult, std::string& OutError)
	{
		const char* Name = bMax ? "max" : "min";
		const std::vector<FValue>* Items = &Args;
		if (Args.size() == 1)
		{
			if (Args[0].Type != EValueType::List)
			{
				OutError = std::string("TypeError: ") + Name + "() expects a list or several numbers";
				return false;
			}
			Items = Args[0].ListValue.get();
		}
		if (Items->empty())
		{
			OutError = std::string("ValueError: ") + Name + "() of an empty list";
			return false;
		}
		const FValue* Best = nullptr;
		for (const FValue& Item : *Items)
		{
			if (Item.Type != EValueType::Number)
			{
				OutError = std::string("TypeError: ") + Name + "() only works with numbers";
				return false;
			}
			if (!Best || (bMax ? Item.NumberValue > Best->NumberValue : Item.NumberValue < Best->NumberValue))
			{
				Best = &Item;
			}
		}
		OutResult = *Best;
		return true;
	}

	void AddStandardLibrary(FBuiltinTable& Table, FPrintFn Print, uint32_t* RandomState)
	{
		Table.AddFunction("print", 0, 16, [Print](const std::vector<FValue>& Args, FValue& OutResult, std::string&)
		{
			std::string Line;
			for (size_t i = 0; i < Args.size(); ++i)
			{
				if (i > 0)
				{
					Line += " ";
				}
				Line += Args[i].ToDisplayString();
			}
			if (Print)
			{
				Print(Line);
			}
			OutResult = FValue::MakeNone();
			return ECallResult::Ok;
		});

		Table.AddFunction("range", 1, 3, [](const std::vector<FValue>& Args, FValue& OutResult, std::string& OutError)
		{
			long long Start = 0;
			long long Stop = 0;
			long long Step = 1;
			if (Args.size() == 1)
			{
				if (!ArgToInt(Args[0], "range", Stop, OutError)) return ECallResult::Error;
			}
			else
			{
				if (!ArgToInt(Args[0], "range", Start, OutError)) return ECallResult::Error;
				if (!ArgToInt(Args[1], "range", Stop, OutError)) return ECallResult::Error;
				if (Args.size() == 3 && !ArgToInt(Args[2], "range", Step, OutError)) return ECallResult::Error;
			}
			if (Step == 0)
			{
				OutError = "ValueError: range() step must not be zero";
				return ECallResult::Error;
			}
			std::vector<FValue> Items;
			for (long long i = Start; Step > 0 ? i < Stop : i > Stop; i += Step)
			{
				if (Items.size() >= MaxListSize)
				{
					OutError = "MemoryError: range() is too big";
					return ECallResult::Error;
				}
				Items.push_back(FValue::MakeNumber(static_cast<double>(i)));
			}
			OutResult = FValue::MakeList(std::move(Items));
			return ECallResult::Ok;
		});

		Table.AddFunction("len", 1, 1, [](const std::vector<FValue>& Args, FValue& OutResult, std::string& OutError)
		{
			if (Args[0].Type == EValueType::List)
			{
				OutResult = FValue::MakeNumber(static_cast<double>(Args[0].ListValue->size()));
				return ECallResult::Ok;
			}
			if (Args[0].Type == EValueType::String)
			{
				OutResult = FValue::MakeNumber(static_cast<double>(Args[0].StringValue.size()));
				return ECallResult::Ok;
			}
			OutError = std::string("TypeError: object of type '") + Args[0].TypeName() + "' has no len()";
			return ECallResult::Error;
		});

		Table.AddFunction("str", 1, 1, [](const std::vector<FValue>& Args, FValue& OutResult, std::string&)
		{
			OutResult = FValue::MakeString(Args[0].ToDisplayString());
			return ECallResult::Ok;
		});

		Table.AddFunction("int", 1, 1, [](const std::vector<FValue>& Args, FValue& OutResult, std::string& OutError)
		{
			const FValue& A = Args[0];
			if (A.Type == EValueType::Number)
			{
				OutResult = FValue::MakeNumber(std::trunc(A.NumberValue));
				return ECallResult::Ok;
			}
			if (A.Type == EValueType::Bool)
			{
				OutResult = FValue::MakeNumber(A.BoolValue ? 1.0 : 0.0);
				return ECallResult::Ok;
			}
			if (A.Type == EValueType::String)
			{
				char* End = nullptr;
				const double Parsed = std::strtod(A.StringValue.c_str(), &End);
				if (End && End != A.StringValue.c_str() && *End == '\0')
				{
					OutResult = FValue::MakeNumber(std::trunc(Parsed));
					return ECallResult::Ok;
				}
				OutError = "ValueError: invalid literal for int(): '" + A.StringValue + "'";
				return ECallResult::Error;
			}
			OutError = std::string("TypeError: int() can't convert ") + A.TypeName();
			return ECallResult::Error;
		});

		Table.AddFunction("abs", 1, 1, [](const std::vector<FValue>& Args, FValue& OutResult, std::string& OutError)
		{
			if (Args[0].Type != EValueType::Number)
			{
				OutError = std::string("TypeError: bad operand type for abs(): '") + Args[0].TypeName() + "'";
				return ECallResult::Error;
			}
			OutResult = FValue::MakeNumber(std::fabs(Args[0].NumberValue));
			return ECallResult::Ok;
		});

		Table.AddFunction("min", 1, 16, [](const std::vector<FValue>& Args, FValue& OutResult, std::string& OutError)
		{
			return MinMaxImpl(Args, false, OutResult, OutError) ? ECallResult::Ok : ECallResult::Error;
		});

		Table.AddFunction("max", 1, 16, [](const std::vector<FValue>& Args, FValue& OutResult, std::string& OutError)
		{
			return MinMaxImpl(Args, true, OutResult, OutError) ? ECallResult::Ok : ECallResult::Error;
		});

		Table.AddFunction("random", 0, 0, [RandomState](const std::vector<FValue>&, FValue& OutResult, std::string&)
		{
			OutResult = FValue::MakeNumber(static_cast<double>(NextRandom(RandomState) % 1000000u) / 1000000.0);
			return ECallResult::Ok;
		});

		Table.AddFunction("randint", 2, 2, [RandomState](const std::vector<FValue>& Args, FValue& OutResult, std::string& OutError)
		{
			long long Low = 0;
			long long High = 0;
			if (!ArgToInt(Args[0], "randint", Low, OutError) || !ArgToInt(Args[1], "randint", High, OutError))
			{
				return ECallResult::Error;
			}
			if (High < Low)
			{
				OutError = "ValueError: randint(a, b) needs a <= b";
				return ECallResult::Error;
			}
			const unsigned long long Span = static_cast<unsigned long long>(High - Low) + 1ull;
			OutResult = FValue::MakeNumber(static_cast<double>(Low + static_cast<long long>(NextRandom(RandomState) % Span)));
			return ECallResult::Ok;
		});
	}

	// ------------------------------------------------------------------
	// Lexer
	// ------------------------------------------------------------------

	namespace ScriptInternal
	{
		enum class ETok : uint8_t
		{
			Name,
			Keyword,
			Number,
			String,
			Op,
			Newline,
			Indent,
			Dedent,
			End
		};

		struct FToken
		{
			ETok Kind = ETok::End;
			std::string Text;
			double Num = 0.0;
			int Line = 0;
		};

		static bool IsKeywordText(const std::string& Text)
		{
			static const char* const Keywords[] = {
				"if", "elif", "else", "while", "for", "in", "def", "return", "break", "continue",
				"pass", "and", "or", "not", "True", "False", "None", "global"
			};
			for (const char* Keyword : Keywords)
			{
				if (Text == Keyword)
				{
					return true;
				}
			}
			return false;
		}

		static bool IsIdentStart(char C)
		{
			return (C >= 'a' && C <= 'z') || (C >= 'A' && C <= 'Z') || C == '_';
		}

		static bool IsDigit(char C)
		{
			return C >= '0' && C <= '9';
		}

		class FLexer
		{
		public:
			explicit FLexer(const std::string& InSource) : Source(InSource) {}

			bool Tokenize(std::vector<FToken>& Out, FCompileError& OutError)
			{
				std::vector<int> Indents{0};
				int ParenDepth = 0;
				bool bLineStart = true;
				const size_t Size = Source.size();

				while (true)
				{
					if (bLineStart && ParenDepth == 0)
					{
						int Width = 0;
						size_t P = Pos;
						while (P < Size && (Source[P] == ' ' || Source[P] == '\t'))
						{
							Width += Source[P] == '\t' ? 4 : 1;
							++P;
						}
						if (P >= Size)
						{
							Pos = P;
							break;
						}
						const char First = Source[P];
						if (First == '\n' || First == '\r' || First == '#')
						{
							// Blank or comment-only line: does not affect indentation.
							while (P < Size && Source[P] != '\n')
							{
								++P;
							}
							if (P < Size)
							{
								++P;
								++Line;
							}
							Pos = P;
							continue;
						}
						Pos = P;
						bLineStart = false;
						if (Width > Indents.back())
						{
							Indents.push_back(Width);
							Push(Out, ETok::Indent, "");
						}
						else
						{
							while (Width < Indents.back())
							{
								Indents.pop_back();
								Push(Out, ETok::Dedent, "");
							}
							if (Width != Indents.back())
							{
								return Fail(OutError, "IndentationError: unindent does not match any outer indentation level");
							}
						}
					}

					if (Pos >= Size)
					{
						break;
					}

					const char C = Source[Pos];
					if (C == '\n')
					{
						if (ParenDepth == 0 && !Out.empty() && Out.back().Kind != ETok::Newline)
						{
							Push(Out, ETok::Newline, "");
						}
						++Line;
						++Pos;
						bLineStart = true;
						continue;
					}
					if (C == ' ' || C == '\t' || C == '\r')
					{
						++Pos;
						continue;
					}
					if (C == '#')
					{
						while (Pos < Size && Source[Pos] != '\n')
						{
							++Pos;
						}
						continue;
					}
					if (C == '\\' && Pos + 1 < Size && Source[Pos + 1] == '\n')
					{
						Pos += 2;
						++Line;
						continue;
					}
					if (IsDigit(C) || (C == '.' && Pos + 1 < Size && IsDigit(Source[Pos + 1])))
					{
						const size_t Start = Pos;
						while (Pos < Size && (IsDigit(Source[Pos]) || Source[Pos] == '.'))
						{
							++Pos;
						}
						const std::string Text = Source.substr(Start, Pos - Start);
						char* End = nullptr;
						const double Value = std::strtod(Text.c_str(), &End);
						if (!End || *End != '\0')
						{
							return Fail(OutError, "SyntaxError: invalid number '" + Text + "'");
						}
						FToken Token;
						Token.Kind = ETok::Number;
						Token.Text = Text;
						Token.Num = Value;
						Token.Line = Line;
						Out.push_back(Token);
						continue;
					}
					if (IsIdentStart(C))
					{
						const size_t Start = Pos;
						while (Pos < Size && (IsIdentStart(Source[Pos]) || IsDigit(Source[Pos])))
						{
							++Pos;
						}
						const std::string Text = Source.substr(Start, Pos - Start);
						Push(Out, IsKeywordText(Text) ? ETok::Keyword : ETok::Name, Text);
						continue;
					}
					if (C == '"' || C == '\'')
					{
						const char Quote = C;
						++Pos;
						std::string Text;
						bool bClosed = false;
						while (Pos < Size)
						{
							const char S = Source[Pos];
							if (S == Quote)
							{
								++Pos;
								bClosed = true;
								break;
							}
							if (S == '\n')
							{
								break;
							}
							if (S == '\\' && Pos + 1 < Size)
							{
								const char E = Source[Pos + 1];
								Pos += 2;
								switch (E)
								{
								case 'n': Text += '\n'; break;
								case 't': Text += '\t'; break;
								case '\\': Text += '\\'; break;
								case '"': Text += '"'; break;
								case '\'': Text += '\''; break;
								default: Text += '\\'; Text += E; break;
								}
								continue;
							}
							Text += S;
							++Pos;
						}
						if (!bClosed)
						{
							return Fail(OutError, "SyntaxError: unterminated string");
						}
						Push(Out, ETok::String, Text);
						continue;
					}

					static const char* const TwoCharOps[] = { "==", "!=", "<=", ">=", "//", "+=", "-=", "*=", "/=" };
					bool bMatchedTwo = false;
					if (Pos + 1 < Size)
					{
						for (const char* Op : TwoCharOps)
						{
							if (Source[Pos] == Op[0] && Source[Pos + 1] == Op[1])
							{
								Push(Out, ETok::Op, std::string(Op));
								Pos += 2;
								bMatchedTwo = true;
								break;
							}
						}
					}
					if (bMatchedTwo)
					{
						continue;
					}

					static const std::string SingleOps = "+-*/%<>=()[],:.";
					if (SingleOps.find(C) != std::string::npos)
					{
						if (C == '(' || C == '[')
						{
							++ParenDepth;
						}
						else if ((C == ')' || C == ']') && ParenDepth > 0)
						{
							--ParenDepth;
						}
						Push(Out, ETok::Op, std::string(1, C));
						++Pos;
						continue;
					}

					if (static_cast<unsigned char>(C) >= 128)
					{
						return Fail(OutError, "SyntaxError: only plain ASCII characters are allowed in code (no accents)");
					}
					return Fail(OutError, std::string("SyntaxError: invalid character '") + C + "'");
				}

				if (!Out.empty() && Out.back().Kind != ETok::Newline)
				{
					Push(Out, ETok::Newline, "");
				}
				while (Indents.size() > 1)
				{
					Indents.pop_back();
					Push(Out, ETok::Dedent, "");
				}
				Push(Out, ETok::End, "");
				return true;
			}

		private:
			void Push(std::vector<FToken>& Out, ETok Kind, const std::string& Text)
			{
				FToken Token;
				Token.Kind = Kind;
				Token.Text = Text;
				Token.Line = Line;
				Out.push_back(Token);
			}

			bool Fail(FCompileError& OutError, const std::string& Message)
			{
				OutError.Line = Line;
				OutError.Message = Message;
				return false;
			}

			const std::string& Source;
			size_t Pos = 0;
			int Line = 1;
		};

		// --------------------------------------------------------------
		// AST
		// --------------------------------------------------------------

		enum class EExprKind : uint8_t
		{
			Number, String, Bool, NoneLit, Name, List, Index, Call, Method, Negate, Not, Binary, And, Or
		};

		struct FExpr
		{
			EExprKind Kind = EExprKind::NoneLit;
			int Line = 0;
			double Num = 0.0;
			bool BoolLit = false;
			std::string Text;
			EBinaryOp Op = EBinaryOp::Add;
			std::vector<std::unique_ptr<FExpr>> Kids;
		};
		using FExprPtr = std::unique_ptr<FExpr>;

		enum class EStmtKind : uint8_t
		{
			ExprStmt, Assign, AugAssign, If, While, For, Def, Return, Break, Continue, Pass, Global
		};

		struct FStmt
		{
			EStmtKind Kind = EStmtKind::Pass;
			int Line = 0;
			std::string Name;
			EBinaryOp Op = EBinaryOp::Add;
			std::vector<std::string> Names;
			FExprPtr Target;
			FExprPtr Value;
			std::vector<std::unique_ptr<FStmt>> Body;
			std::vector<std::unique_ptr<FStmt>> Else;
		};
		using FStmtPtr = std::unique_ptr<FStmt>;
		using FStmtList = std::vector<FStmtPtr>;

		// --------------------------------------------------------------
		// Parser (recursive descent; returns nullptr / false on error)
		// --------------------------------------------------------------

		class FParser
		{
		public:
			FParser(const std::vector<FToken>& InTokens, FCompileError& InError) : Tokens(InTokens), Error(InError) {}

			bool ParseProgram(FStmtList& Out)
			{
				while (Peek().Kind != ETok::End)
				{
					FStmtPtr Stmt = ParseStatement();
					if (!Stmt)
					{
						return false;
					}
					Out.push_back(std::move(Stmt));
				}
				return true;
			}

		private:
			const FToken& Peek(size_t Offset = 0) const
			{
				const size_t At = Pos + Offset;
				return At < Tokens.size() ? Tokens[At] : Tokens.back();
			}

			const FToken& Advance()
			{
				const FToken& Token = Tokens[Pos];
				if (Pos + 1 < Tokens.size())
				{
					++Pos;
				}
				return Token;
			}

			bool IsOp(const char* Text, size_t Offset = 0) const
			{
				const FToken& Token = Peek(Offset);
				return Token.Kind == ETok::Op && Token.Text == Text;
			}

			bool IsKw(const char* Text, size_t Offset = 0) const
			{
				const FToken& Token = Peek(Offset);
				return Token.Kind == ETok::Keyword && Token.Text == Text;
			}

			bool Fail(const std::string& Message)
			{
				if (Error.Message.empty())
				{
					Error.Line = Peek().Line;
					Error.Message = Message;
				}
				return false;
			}

			std::string Describe(const FToken& Token) const
			{
				switch (Token.Kind)
				{
				case ETok::Newline: return "end of line";
				case ETok::Indent: return "indentation";
				case ETok::Dedent: return "end of block";
				case ETok::End: return "end of file";
				case ETok::String: return "string";
				default: return "'" + Token.Text + "'";
				}
			}

			bool ExpectOp(const char* Text)
			{
				if (!IsOp(Text))
				{
					return Fail(std::string("SyntaxError: expected '") + Text + "' but found " + Describe(Peek()));
				}
				Advance();
				return true;
			}

			bool ExpectNewline()
			{
				if (Peek().Kind == ETok::Newline)
				{
					Advance();
					return true;
				}
				if (Peek().Kind == ETok::End)
				{
					return true;
				}
				return Fail("SyntaxError: unexpected " + Describe(Peek()) + " (missing operator or new line?)");
			}

			FStmtPtr MakeStmt(EStmtKind Kind, int Line)
			{
				FStmtPtr Stmt = std::make_unique<FStmt>();
				Stmt->Kind = Kind;
				Stmt->Line = Line;
				return Stmt;
			}

			FExprPtr MakeExpr(EExprKind Kind, int Line)
			{
				FExprPtr Expr = std::make_unique<FExpr>();
				Expr->Kind = Kind;
				Expr->Line = Line;
				return Expr;
			}

			FStmtPtr ParseStatement()
			{
				const FToken& Token = Peek();
				if (Token.Kind == ETok::Indent)
				{
					Fail("IndentationError: unexpected indent");
					return nullptr;
				}
				if (Token.Kind == ETok::Keyword)
				{
					if (Token.Text == "if") return ParseIf();
					if (Token.Text == "while") return ParseWhile();
					if (Token.Text == "for") return ParseFor();
					if (Token.Text == "def") return ParseDef();
					if (Token.Text == "elif" || Token.Text == "else")
					{
						Fail("SyntaxError: '" + Token.Text + "' without a matching 'if' (check the indentation)");
						return nullptr;
					}
				}
				FStmtPtr Stmt = ParseSimple();
				if (!Stmt || !ExpectNewline())
				{
					return nullptr;
				}
				return Stmt;
			}

			bool ParseBlock(FStmtList& Out)
			{
				if (!ExpectOp(":"))
				{
					return false;
				}
				if (Peek().Kind != ETok::Newline)
				{
					// One-line body: "if x: attack()"
					FStmtPtr Stmt = ParseSimple();
					if (!Stmt || !ExpectNewline())
					{
						return false;
					}
					Out.push_back(std::move(Stmt));
					return true;
				}
				Advance();
				if (Peek().Kind != ETok::Indent)
				{
					return Fail("IndentationError: expected an indented block");
				}
				Advance();
				while (Peek().Kind != ETok::Dedent && Peek().Kind != ETok::End)
				{
					FStmtPtr Stmt = ParseStatement();
					if (!Stmt)
					{
						return false;
					}
					Out.push_back(std::move(Stmt));
				}
				if (Peek().Kind == ETok::Dedent)
				{
					Advance();
				}
				return true;
			}

			FStmtPtr ParseIf()
			{
				const int Line = Advance().Line; // 'if' or 'elif'
				FStmtPtr Stmt = MakeStmt(EStmtKind::If, Line);
				Stmt->Value = ParseExpr();
				if (!Stmt->Value || !ParseBlock(Stmt->Body))
				{
					return nullptr;
				}
				if (IsKw("elif"))
				{
					FStmtPtr Elif = ParseIf();
					if (!Elif)
					{
						return nullptr;
					}
					Stmt->Else.push_back(std::move(Elif));
				}
				else if (IsKw("else"))
				{
					Advance();
					if (!ParseBlock(Stmt->Else))
					{
						return nullptr;
					}
				}
				return Stmt;
			}

			FStmtPtr ParseWhile()
			{
				const int Line = Advance().Line;
				FStmtPtr Stmt = MakeStmt(EStmtKind::While, Line);
				Stmt->Value = ParseExpr();
				if (!Stmt->Value || !ParseBlock(Stmt->Body))
				{
					return nullptr;
				}
				return Stmt;
			}

			FStmtPtr ParseFor()
			{
				const int Line = Advance().Line;
				FStmtPtr Stmt = MakeStmt(EStmtKind::For, Line);
				if (Peek().Kind != ETok::Name)
				{
					Fail("SyntaxError: expected a variable name after 'for'");
					return nullptr;
				}
				Stmt->Name = Advance().Text;
				if (!IsKw("in"))
				{
					Fail("SyntaxError: expected 'in' in for loop");
					return nullptr;
				}
				Advance();
				Stmt->Value = ParseExpr();
				if (!Stmt->Value || !ParseBlock(Stmt->Body))
				{
					return nullptr;
				}
				return Stmt;
			}

			FStmtPtr ParseDef()
			{
				const int Line = Advance().Line;
				FStmtPtr Stmt = MakeStmt(EStmtKind::Def, Line);
				if (Peek().Kind != ETok::Name)
				{
					Fail("SyntaxError: expected a function name after 'def'");
					return nullptr;
				}
				Stmt->Name = Advance().Text;
				if (!ExpectOp("("))
				{
					return nullptr;
				}
				while (!IsOp(")"))
				{
					if (Peek().Kind != ETok::Name)
					{
						Fail("SyntaxError: expected a parameter name");
						return nullptr;
					}
					Stmt->Names.push_back(Advance().Text);
					if (IsOp(","))
					{
						Advance();
					}
					else if (!IsOp(")"))
					{
						Fail("SyntaxError: expected ',' or ')' in parameter list");
						return nullptr;
					}
				}
				Advance();
				if (!ParseBlock(Stmt->Body))
				{
					return nullptr;
				}
				return Stmt;
			}

			FStmtPtr ParseSimple()
			{
				const FToken& Token = Peek();
				const int Line = Token.Line;
				if (Token.Kind == ETok::Keyword)
				{
					if (Token.Text == "pass") { Advance(); return MakeStmt(EStmtKind::Pass, Line); }
					if (Token.Text == "break") { Advance(); return MakeStmt(EStmtKind::Break, Line); }
					if (Token.Text == "continue") { Advance(); return MakeStmt(EStmtKind::Continue, Line); }
					if (Token.Text == "return")
					{
						Advance();
						FStmtPtr Stmt = MakeStmt(EStmtKind::Return, Line);
						if (Peek().Kind != ETok::Newline && Peek().Kind != ETok::End)
						{
							Stmt->Value = ParseExpr();
							if (!Stmt->Value)
							{
								return nullptr;
							}
						}
						return Stmt;
					}
					if (Token.Text == "global")
					{
						Advance();
						FStmtPtr Stmt = MakeStmt(EStmtKind::Global, Line);
						do
						{
							if (Peek().Kind != ETok::Name)
							{
								Fail("SyntaxError: expected a name after 'global'");
								return nullptr;
							}
							Stmt->Names.push_back(Advance().Text);
						}
						while (IsOp(",") && (Advance(), true));
						return Stmt;
					}
				}

				FExprPtr Expr = ParseExpr();
				if (!Expr)
				{
					return nullptr;
				}

				static const struct { const char* Text; EBinaryOp Op; } AugOps[] = {
					{ "+=", EBinaryOp::Add }, { "-=", EBinaryOp::Sub }, { "*=", EBinaryOp::Mul }, { "/=", EBinaryOp::Div }
				};

				if (IsOp("="))
				{
					Advance();
					if (Expr->Kind != EExprKind::Name && Expr->Kind != EExprKind::Index)
					{
						Fail("SyntaxError: can only assign to a variable or a list element");
						return nullptr;
					}
					FStmtPtr Stmt = MakeStmt(EStmtKind::Assign, Line);
					Stmt->Target = std::move(Expr);
					Stmt->Value = ParseExpr();
					if (!Stmt->Value)
					{
						return nullptr;
					}
					return Stmt;
				}
				for (const auto& Aug : AugOps)
				{
					if (IsOp(Aug.Text))
					{
						Advance();
						if (Expr->Kind != EExprKind::Name && Expr->Kind != EExprKind::Index)
						{
							Fail("SyntaxError: can only assign to a variable or a list element");
							return nullptr;
						}
						FStmtPtr Stmt = MakeStmt(EStmtKind::AugAssign, Line);
						Stmt->Op = Aug.Op;
						Stmt->Target = std::move(Expr);
						Stmt->Value = ParseExpr();
						if (!Stmt->Value)
						{
							return nullptr;
						}
						return Stmt;
					}
				}

				FStmtPtr Stmt = MakeStmt(EStmtKind::ExprStmt, Line);
				Stmt->Value = std::move(Expr);
				return Stmt;
			}

			FExprPtr ParseExpr()
			{
				return ParseOr();
			}

			FExprPtr ParseOr()
			{
				FExprPtr Left = ParseAnd();
				while (Left && IsKw("or"))
				{
					const int Line = Advance().Line;
					FExprPtr Right = ParseAnd();
					if (!Right)
					{
						return nullptr;
					}
					FExprPtr Node = MakeExpr(EExprKind::Or, Line);
					Node->Kids.push_back(std::move(Left));
					Node->Kids.push_back(std::move(Right));
					Left = std::move(Node);
				}
				return Left;
			}

			FExprPtr ParseAnd()
			{
				FExprPtr Left = ParseNot();
				while (Left && IsKw("and"))
				{
					const int Line = Advance().Line;
					FExprPtr Right = ParseNot();
					if (!Right)
					{
						return nullptr;
					}
					FExprPtr Node = MakeExpr(EExprKind::And, Line);
					Node->Kids.push_back(std::move(Left));
					Node->Kids.push_back(std::move(Right));
					Left = std::move(Node);
				}
				return Left;
			}

			FExprPtr ParseNot()
			{
				if (IsKw("not"))
				{
					const int Line = Advance().Line;
					FExprPtr Operand = ParseNot();
					if (!Operand)
					{
						return nullptr;
					}
					FExprPtr Node = MakeExpr(EExprKind::Not, Line);
					Node->Kids.push_back(std::move(Operand));
					return Node;
				}
				return ParseComparison();
			}

			bool MatchComparison(EBinaryOp& OutOp)
			{
				static const struct { const char* Text; EBinaryOp Op; } CompareOps[] = {
					{ "==", EBinaryOp::Eq }, { "!=", EBinaryOp::NotEq }, { "<", EBinaryOp::Less },
					{ "<=", EBinaryOp::LessEq }, { ">", EBinaryOp::Greater }, { ">=", EBinaryOp::GreaterEq }
				};
				for (const auto& Compare : CompareOps)
				{
					if (IsOp(Compare.Text))
					{
						Advance();
						OutOp = Compare.Op;
						return true;
					}
				}
				if (IsKw("in"))
				{
					Advance();
					OutOp = EBinaryOp::In;
					return true;
				}
				if (IsKw("not") && IsKw("in", 1))
				{
					Advance();
					Advance();
					OutOp = EBinaryOp::NotIn;
					return true;
				}
				return false;
			}

			FExprPtr ParseComparison()
			{
				FExprPtr Left = ParseArith();
				EBinaryOp Op = EBinaryOp::Eq;
				while (Left)
				{
					const int Line = Peek().Line;
					if (!MatchComparison(Op))
					{
						break;
					}
					FExprPtr Right = ParseArith();
					if (!Right)
					{
						return nullptr;
					}
					FExprPtr Node = MakeExpr(EExprKind::Binary, Line);
					Node->Op = Op;
					Node->Kids.push_back(std::move(Left));
					Node->Kids.push_back(std::move(Right));
					Left = std::move(Node);
				}
				return Left;
			}

			FExprPtr ParseArith()
			{
				FExprPtr Left = ParseTerm();
				while (Left && (IsOp("+") || IsOp("-")))
				{
					const FToken& OpToken = Advance();
					FExprPtr Right = ParseTerm();
					if (!Right)
					{
						return nullptr;
					}
					FExprPtr Node = MakeExpr(EExprKind::Binary, OpToken.Line);
					Node->Op = OpToken.Text == "+" ? EBinaryOp::Add : EBinaryOp::Sub;
					Node->Kids.push_back(std::move(Left));
					Node->Kids.push_back(std::move(Right));
					Left = std::move(Node);
				}
				return Left;
			}

			FExprPtr ParseTerm()
			{
				FExprPtr Left = ParseUnary();
				while (Left && (IsOp("*") || IsOp("/") || IsOp("//") || IsOp("%")))
				{
					const FToken& OpToken = Advance();
					FExprPtr Right = ParseUnary();
					if (!Right)
					{
						return nullptr;
					}
					FExprPtr Node = MakeExpr(EExprKind::Binary, OpToken.Line);
					if (OpToken.Text == "*") Node->Op = EBinaryOp::Mul;
					else if (OpToken.Text == "/") Node->Op = EBinaryOp::Div;
					else if (OpToken.Text == "//") Node->Op = EBinaryOp::FloorDiv;
					else Node->Op = EBinaryOp::Mod;
					Node->Kids.push_back(std::move(Left));
					Node->Kids.push_back(std::move(Right));
					Left = std::move(Node);
				}
				return Left;
			}

			FExprPtr ParseUnary()
			{
				if (IsOp("-"))
				{
					const int Line = Advance().Line;
					FExprPtr Operand = ParseUnary();
					if (!Operand)
					{
						return nullptr;
					}
					FExprPtr Node = MakeExpr(EExprKind::Negate, Line);
					Node->Kids.push_back(std::move(Operand));
					return Node;
				}
				if (IsOp("+"))
				{
					Advance();
					return ParseUnary();
				}
				return ParsePostfix();
			}

			bool ParseArgs(std::vector<FExprPtr>& Out)
			{
				// '(' already consumed
				while (!IsOp(")"))
				{
					FExprPtr Arg = ParseExpr();
					if (!Arg)
					{
						return false;
					}
					Out.push_back(std::move(Arg));
					if (IsOp(","))
					{
						Advance();
					}
					else if (!IsOp(")"))
					{
						return Fail("SyntaxError: expected ',' or ')' in call, found " + Describe(Peek()));
					}
				}
				Advance();
				return true;
			}

			FExprPtr ParsePostfix()
			{
				FExprPtr Expr = ParseAtom();
				while (Expr)
				{
					if (IsOp("("))
					{
						const int Line = Advance().Line;
						FExprPtr Node = MakeExpr(EExprKind::Call, Line);
						Node->Kids.push_back(std::move(Expr));
						if (!ParseArgs(Node->Kids))
						{
							return nullptr;
						}
						Expr = std::move(Node);
					}
					else if (IsOp("["))
					{
						const int Line = Advance().Line;
						FExprPtr Key = ParseExpr();
						if (!Key || !ExpectOp("]"))
						{
							return nullptr;
						}
						FExprPtr Node = MakeExpr(EExprKind::Index, Line);
						Node->Kids.push_back(std::move(Expr));
						Node->Kids.push_back(std::move(Key));
						Expr = std::move(Node);
					}
					else if (IsOp("."))
					{
						const int Line = Advance().Line;
						if (Peek().Kind != ETok::Name)
						{
							Fail("SyntaxError: expected a method name after '.'");
							return nullptr;
						}
						FExprPtr Node = MakeExpr(EExprKind::Method, Line);
						Node->Text = Advance().Text;
						if (!ExpectOp("("))
						{
							return nullptr;
						}
						Node->Kids.push_back(std::move(Expr));
						if (!ParseArgs(Node->Kids))
						{
							return nullptr;
						}
						Expr = std::move(Node);
					}
					else
					{
						break;
					}
				}
				return Expr;
			}

			FExprPtr ParseAtom()
			{
				const FToken& Token = Peek();
				const int Line = Token.Line;
				switch (Token.Kind)
				{
				case ETok::Number:
				{
					FExprPtr Node = MakeExpr(EExprKind::Number, Line);
					Node->Num = Advance().Num;
					return Node;
				}
				case ETok::String:
				{
					FExprPtr Node = MakeExpr(EExprKind::String, Line);
					Node->Text = Advance().Text;
					return Node;
				}
				case ETok::Name:
				{
					FExprPtr Node = MakeExpr(EExprKind::Name, Line);
					Node->Text = Advance().Text;
					return Node;
				}
				case ETok::Keyword:
					if (Token.Text == "True" || Token.Text == "False")
					{
						FExprPtr Node = MakeExpr(EExprKind::Bool, Line);
						Node->BoolLit = Advance().Text == "True";
						return Node;
					}
					if (Token.Text == "None")
					{
						Advance();
						return MakeExpr(EExprKind::NoneLit, Line);
					}
					Fail("SyntaxError: unexpected keyword '" + Token.Text + "'");
					return nullptr;
				case ETok::Op:
					if (Token.Text == "(")
					{
						Advance();
						FExprPtr Inner = ParseExpr();
						if (!Inner || !ExpectOp(")"))
						{
							return nullptr;
						}
						return Inner;
					}
					if (Token.Text == "[")
					{
						Advance();
						FExprPtr Node = MakeExpr(EExprKind::List, Line);
						while (!IsOp("]"))
						{
							FExprPtr Item = ParseExpr();
							if (!Item)
							{
								return nullptr;
							}
							Node->Kids.push_back(std::move(Item));
							if (IsOp(","))
							{
								Advance();
							}
							else if (!IsOp("]"))
							{
								Fail("SyntaxError: expected ',' or ']' in list");
								return nullptr;
							}
						}
						Advance();
						return Node;
					}
					break;
				default:
					break;
				}
				Fail("SyntaxError: unexpected " + Describe(Token));
				return nullptr;
			}

			const std::vector<FToken>& Tokens;
			FCompileError& Error;
			size_t Pos = 0;
		};

		// --------------------------------------------------------------
		// Compiler (AST -> bytecode)
		// --------------------------------------------------------------

		class FCompiler
		{
		public:
			FCompiler(const FBuiltinTable& InBuiltins, const FCompileOptions& InOptions, FProgram& InProgram, FCompileError& InError)
				: Builtins(InBuiltins), Options(InOptions), Program(InProgram), Error(InError)
			{
			}

			bool CompileModule(const FStmtList& Stmts)
			{
				CollectModuleNames(Stmts);

				Program.Functions.clear();
				Program.Functions.emplace_back();
				Program.Functions[0].Name = "<main>";

				FFuncCtx Module;
				Module.ProtoIndex = 0;
				Module.bIsModule = true;
				Cur = &Module;

				if (!CompileBlock(Stmts))
				{
					return false;
				}
				Emit(EOp::Halt, 0, 0, LastLine);
				return true;
			}

		private:
			struct FLoopCtx
			{
				bool bIsFor = false;
				int ContinueTarget = 0;
				std::vector<int> BreakJumps;
			};

			struct FFuncCtx
			{
				int ProtoIndex = 0;
				bool bIsModule = false;
				std::unordered_map<std::string, int> Locals;
				std::unordered_set<std::string> DeclaredGlobals;
				std::vector<FLoopCtx> Loops;
			};

			bool Fail(int Line, const std::string& Message)
			{
				if (Error.Message.empty())
				{
					Error.Line = Line;
					Error.Message = Message;
				}
				return false;
			}

			std::vector<FInstr>& Code()
			{
				return Program.Functions[static_cast<size_t>(Cur->ProtoIndex)].Code;
			}

			int Here()
			{
				return static_cast<int>(Code().size());
			}

			int Emit(EOp Op, int A, int B, int Line)
			{
				FInstr Instr;
				Instr.Op = Op;
				Instr.A = A;
				Instr.B = B;
				Instr.Line = Line;
				Code().push_back(Instr);
				LastLine = Line;
				return static_cast<int>(Code().size()) - 1;
			}

			void PatchJump(int At, int Target)
			{
				Code()[static_cast<size_t>(At)].A = Target;
			}

			int AddConstant(const FValue& Value)
			{
				Program.Constants.push_back(Value);
				return static_cast<int>(Program.Constants.size()) - 1;
			}

			int GlobalSlot(const std::string& Name)
			{
				const auto It = GlobalSlots.find(Name);
				if (It != GlobalSlots.end())
				{
					return It->second;
				}
				const int Slot = static_cast<int>(Program.GlobalNames.size());
				Program.GlobalNames.push_back(Name);
				GlobalSlots[Name] = Slot;
				return Slot;
			}

			void CollectModuleNames(const FStmtList& Stmts)
			{
				for (const FStmtPtr& Stmt : Stmts)
				{
					switch (Stmt->Kind)
					{
					case EStmtKind::Assign:
					case EStmtKind::AugAssign:
						if (Stmt->Target->Kind == EExprKind::Name)
						{
							ModuleNames.insert(Stmt->Target->Text);
						}
						break;
					case EStmtKind::For:
						ModuleNames.insert(Stmt->Name);
						CollectModuleNames(Stmt->Body);
						break;
					case EStmtKind::If:
					case EStmtKind::While:
						CollectModuleNames(Stmt->Body);
						CollectModuleNames(Stmt->Else);
						break;
					case EStmtKind::Def:
						ModuleNames.insert(Stmt->Name);
						CollectGlobalDecls(Stmt->Body);
						break;
					default:
						break;
					}
				}
			}

			void CollectGlobalDecls(const FStmtList& Stmts)
			{
				for (const FStmtPtr& Stmt : Stmts)
				{
					if (Stmt->Kind == EStmtKind::Global)
					{
						for (const std::string& Name : Stmt->Names)
						{
							ModuleNames.insert(Name);
						}
					}
					CollectGlobalDecls(Stmt->Body);
					CollectGlobalDecls(Stmt->Else);
				}
			}

			void CollectFunctionLocals(const FStmtList& Stmts, FFuncCtx& Ctx, std::vector<std::string>& Order)
			{
				for (const FStmtPtr& Stmt : Stmts)
				{
					if (Stmt->Kind == EStmtKind::Global)
					{
						for (const std::string& Name : Stmt->Names)
						{
							Ctx.DeclaredGlobals.insert(Name);
						}
					}
				}
				for (const FStmtPtr& Stmt : Stmts)
				{
					switch (Stmt->Kind)
					{
					case EStmtKind::Assign:
					case EStmtKind::AugAssign:
						if (Stmt->Target->Kind == EExprKind::Name)
						{
							Order.push_back(Stmt->Target->Text);
						}
						break;
					case EStmtKind::For:
						Order.push_back(Stmt->Name);
						CollectFunctionLocals(Stmt->Body, Ctx, Order);
						break;
					case EStmtKind::If:
					case EStmtKind::While:
						CollectFunctionLocals(Stmt->Body, Ctx, Order);
						CollectFunctionLocals(Stmt->Else, Ctx, Order);
						break;
					default:
						break;
					}
				}
			}

			void EmitLoadName(const std::string& Name, int Line)
			{
				if (!Cur->bIsModule)
				{
					const auto It = Cur->Locals.find(Name);
					if (It != Cur->Locals.end())
					{
						Emit(EOp::LoadLocal, It->second, 0, Line);
						return;
					}
				}
				if (ModuleNames.count(Name) == 0 && Cur->DeclaredGlobals.count(Name) == 0)
				{
					const int BuiltinIndex = Builtins.Find(Name);
					if (BuiltinIndex >= 0)
					{
						Emit(EOp::LoadBuiltin, BuiltinIndex, 0, Line);
						return;
					}
				}
				Emit(EOp::LoadGlobal, GlobalSlot(Name), 0, Line);
			}

			void EmitStoreName(const std::string& Name, int Line)
			{
				if (!Cur->bIsModule)
				{
					const auto It = Cur->Locals.find(Name);
					if (It != Cur->Locals.end())
					{
						Emit(EOp::StoreLocal, It->second, 0, Line);
						return;
					}
				}
				Emit(EOp::StoreGlobal, GlobalSlot(Name), 0, Line);
			}

			bool CheckLists(int Line)
			{
				if (!Options.bAllowLists)
				{
					return Fail(Line, "Locked: lists are not unlocked yet (buy them in the shop)");
				}
				return true;
			}

			bool CompileBlock(const FStmtList& Stmts)
			{
				for (const FStmtPtr& Stmt : Stmts)
				{
					if (!CompileStmt(*Stmt))
					{
						return false;
					}
				}
				return true;
			}

			bool CompileStmt(const FStmt& Stmt)
			{
				const int Line = Stmt.Line;
				switch (Stmt.Kind)
				{
				case EStmtKind::ExprStmt:
					if (!CompileExpr(*Stmt.Value)) return false;
					Emit(EOp::Pop, 0, 0, Line);
					return true;

				case EStmtKind::Assign:
					if (Stmt.Target->Kind == EExprKind::Name)
					{
						if (!CompileExpr(*Stmt.Value)) return false;
						EmitStoreName(Stmt.Target->Text, Line);
						return true;
					}
					if (!CheckLists(Line)) return false;
					if (!CompileExpr(*Stmt.Target->Kids[0]) || !CompileExpr(*Stmt.Target->Kids[1]) || !CompileExpr(*Stmt.Value)) return false;
					Emit(EOp::StoreIndex, 0, 0, Line);
					return true;

				case EStmtKind::AugAssign:
					if (Stmt.Target->Kind == EExprKind::Name)
					{
						EmitLoadName(Stmt.Target->Text, Line);
						if (!CompileExpr(*Stmt.Value)) return false;
						Emit(EOp::Binary, static_cast<int>(Stmt.Op), 0, Line);
						EmitStoreName(Stmt.Target->Text, Line);
						return true;
					}
					if (!CheckLists(Line)) return false;
					if (!CompileExpr(*Stmt.Target->Kids[0]) || !CompileExpr(*Stmt.Target->Kids[1])) return false;
					Emit(EOp::Dup2, 0, 0, Line);
					Emit(EOp::Index, 0, 0, Line);
					if (!CompileExpr(*Stmt.Value)) return false;
					Emit(EOp::Binary, static_cast<int>(Stmt.Op), 0, Line);
					Emit(EOp::StoreIndex, 0, 0, Line);
					return true;

				case EStmtKind::If:
				{
					if (!CompileExpr(*Stmt.Value)) return false;
					const int JumpElse = Emit(EOp::JumpIfFalse, 0, 0, Line);
					if (!CompileBlock(Stmt.Body)) return false;
					if (Stmt.Else.empty())
					{
						PatchJump(JumpElse, Here());
						return true;
					}
					const int JumpEnd = Emit(EOp::Jump, 0, 0, Line);
					PatchJump(JumpElse, Here());
					if (!CompileBlock(Stmt.Else)) return false;
					PatchJump(JumpEnd, Here());
					return true;
				}

				case EStmtKind::While:
				{
					const int Start = Here();
					if (!CompileExpr(*Stmt.Value)) return false;
					const int JumpEnd = Emit(EOp::JumpIfFalse, 0, 0, Line);
					FLoopCtx Loop;
					Loop.ContinueTarget = Start;
					Cur->Loops.push_back(Loop);
					if (!CompileBlock(Stmt.Body)) return false;
					Emit(EOp::Jump, Start, 0, Line);
					const int End = Here();
					PatchJump(JumpEnd, End);
					for (const int Jump : Cur->Loops.back().BreakJumps)
					{
						PatchJump(Jump, End);
					}
					Cur->Loops.pop_back();
					return true;
				}

				case EStmtKind::For:
				{
					if (!Options.bAllowFor)
					{
						return Fail(Line, "Locked: 'for' loops are not unlocked yet (buy them in the shop)");
					}
					if (!CompileExpr(*Stmt.Value)) return false;
					Emit(EOp::GetIter, 0, 0, Line);
					const int LoopTop = Emit(EOp::ForIter, 0, 0, Line);
					EmitStoreName(Stmt.Name, Line);
					FLoopCtx Loop;
					Loop.bIsFor = true;
					Loop.ContinueTarget = LoopTop;
					Cur->Loops.push_back(Loop);
					if (!CompileBlock(Stmt.Body)) return false;
					Emit(EOp::Jump, LoopTop, 0, Line);
					const int End = Here();
					PatchJump(LoopTop, End);
					for (const int Jump : Cur->Loops.back().BreakJumps)
					{
						PatchJump(Jump, End);
					}
					Cur->Loops.pop_back();
					return true;
				}

				case EStmtKind::Break:
				case EStmtKind::Continue:
				{
					if (Cur->Loops.empty())
					{
						return Fail(Line, std::string("SyntaxError: '") + (Stmt.Kind == EStmtKind::Break ? "break" : "continue") + "' outside loop");
					}
					FLoopCtx& Loop = Cur->Loops.back();
					if (Stmt.Kind == EStmtKind::Continue)
					{
						Emit(EOp::Jump, Loop.ContinueTarget, 0, Line);
						return true;
					}
					if (Loop.bIsFor)
					{
						// Drop the iterator (list + index) before leaving the loop.
						Emit(EOp::Pop, 0, 0, Line);
						Emit(EOp::Pop, 0, 0, Line);
					}
					Loop.BreakJumps.push_back(Emit(EOp::Jump, 0, 0, Line));
					return true;
				}

				case EStmtKind::Def:
					return CompileDef(Stmt);

				case EStmtKind::Return:
					if (Cur->bIsModule)
					{
						return Fail(Line, "SyntaxError: 'return' outside function");
					}
					if (Stmt.Value)
					{
						if (!CompileExpr(*Stmt.Value)) return false;
					}
					else
					{
						Emit(EOp::Const, AddConstant(FValue::MakeNone()), 0, Line);
					}
					Emit(EOp::Return, 0, 0, Line);
					return true;

				case EStmtKind::Pass:
				case EStmtKind::Global:
					return true;
				}
				return Fail(Line, "internal error: unknown statement");
			}

			bool CompileDef(const FStmt& Stmt)
			{
				const int Line = Stmt.Line;
				if (!Options.bAllowDef)
				{
					return Fail(Line, "Locked: 'def' (functions) is not unlocked yet (buy it in the shop)");
				}
				if (!Cur->bIsModule)
				{
					return Fail(Line, "SyntaxError: functions can only be defined at the top level");
				}

				const int ProtoIndex = static_cast<int>(Program.Functions.size());
				Program.Functions.emplace_back();
				FFunctionProto& Proto = Program.Functions.back();
				Proto.Name = Stmt.Name;
				Proto.NumParams = static_cast<int>(Stmt.Names.size());

				FFuncCtx Func;
				Func.ProtoIndex = ProtoIndex;
				std::vector<std::string> Order = Stmt.Names;
				CollectFunctionLocals(Stmt.Body, Func, Order);
				for (const std::string& Name : Order)
				{
					if (Func.DeclaredGlobals.count(Name) == 0 && Func.Locals.count(Name) == 0)
					{
						const int Slot = static_cast<int>(Func.Locals.size());
						Func.Locals[Name] = Slot;
					}
				}
				Program.Functions[static_cast<size_t>(ProtoIndex)].NumLocals = static_cast<int>(Func.Locals.size());

				FFuncCtx* Outer = Cur;
				Cur = &Func;
				const bool bOk = CompileBlock(Stmt.Body);
				if (bOk)
				{
					Emit(EOp::Const, AddConstant(FValue::MakeNone()), 0, LastLine);
					Emit(EOp::Return, 0, 0, LastLine);
				}
				Cur = Outer;
				if (!bOk)
				{
					return false;
				}

				Emit(EOp::Const, AddConstant(FValue::MakeFunction(ProtoIndex)), 0, Line);
				EmitStoreName(Stmt.Name, Line);
				return true;
			}

			bool CompileExpr(const FExpr& Expr)
			{
				const int Line = Expr.Line;
				switch (Expr.Kind)
				{
				case EExprKind::Number:
					Emit(EOp::Const, AddConstant(FValue::MakeNumber(Expr.Num)), 0, Line);
					return true;
				case EExprKind::String:
					Emit(EOp::Const, AddConstant(FValue::MakeString(Expr.Text)), 0, Line);
					return true;
				case EExprKind::Bool:
					Emit(EOp::Const, AddConstant(FValue::MakeBool(Expr.BoolLit)), 0, Line);
					return true;
				case EExprKind::NoneLit:
					Emit(EOp::Const, AddConstant(FValue::MakeNone()), 0, Line);
					return true;
				case EExprKind::Name:
					EmitLoadName(Expr.Text, Line);
					return true;
				case EExprKind::List:
					if (!CheckLists(Line)) return false;
					for (const FExprPtr& Item : Expr.Kids)
					{
						if (!CompileExpr(*Item)) return false;
					}
					Emit(EOp::MakeList, static_cast<int>(Expr.Kids.size()), 0, Line);
					return true;
				case EExprKind::Index:
					if (!CheckLists(Line)) return false;
					if (!CompileExpr(*Expr.Kids[0]) || !CompileExpr(*Expr.Kids[1])) return false;
					Emit(EOp::Index, 0, 0, Line);
					return true;
				case EExprKind::Call:
					for (const FExprPtr& Kid : Expr.Kids)
					{
						if (!CompileExpr(*Kid)) return false;
					}
					Emit(EOp::Call, static_cast<int>(Expr.Kids.size()) - 1, 0, Line);
					return true;
				case EExprKind::Method:
					if (!CheckLists(Line)) return false;
					for (const FExprPtr& Kid : Expr.Kids)
					{
						if (!CompileExpr(*Kid)) return false;
					}
					Emit(EOp::CallMethod, AddConstant(FValue::MakeString(Expr.Text)), static_cast<int>(Expr.Kids.size()) - 1, Line);
					return true;
				case EExprKind::Negate:
					if (!CompileExpr(*Expr.Kids[0])) return false;
					Emit(EOp::Negate, 0, 0, Line);
					return true;
				case EExprKind::Not:
					if (!CompileExpr(*Expr.Kids[0])) return false;
					Emit(EOp::Not, 0, 0, Line);
					return true;
				case EExprKind::Binary:
					if (!CompileExpr(*Expr.Kids[0]) || !CompileExpr(*Expr.Kids[1])) return false;
					Emit(EOp::Binary, static_cast<int>(Expr.Op), 0, Line);
					return true;
				case EExprKind::And:
				case EExprKind::Or:
				{
					if (!CompileExpr(*Expr.Kids[0])) return false;
					const int Jump = Emit(Expr.Kind == EExprKind::And ? EOp::JumpIfFalseOrPop : EOp::JumpIfTrueOrPop, 0, 0, Line);
					if (!CompileExpr(*Expr.Kids[1])) return false;
					PatchJump(Jump, Here());
					return true;
				}
				}
				return Fail(Line, "internal error: unknown expression");
			}

			const FBuiltinTable& Builtins;
			const FCompileOptions& Options;
			FProgram& Program;
			FCompileError& Error;
			FFuncCtx* Cur = nullptr;
			std::unordered_set<std::string> ModuleNames;
			std::unordered_map<std::string, int> GlobalSlots;
			int LastLine = 1;
		};
	}

	bool Compile(const std::string& Source, const FBuiltinTable& Builtins, const FCompileOptions& Options,
		FProgram& OutProgram, FCompileError& OutError)
	{
		OutError = FCompileError();
		OutProgram = FProgram();

		std::vector<ScriptInternal::FToken> Tokens;
		ScriptInternal::FLexer Lexer(Source);
		if (!Lexer.Tokenize(Tokens, OutError))
		{
			return false;
		}

		ScriptInternal::FStmtList Stmts;
		ScriptInternal::FParser Parser(Tokens, OutError);
		if (!Parser.ParseProgram(Stmts))
		{
			return false;
		}

		ScriptInternal::FCompiler Compiler(Builtins, Options, OutProgram, OutError);
		return Compiler.CompileModule(Stmts);
	}

	// ------------------------------------------------------------------
	// VM
	// ------------------------------------------------------------------

	void FVM::Load(std::shared_ptr<const FProgram> InProgram, FBuiltinTable* InBuiltins)
	{
		Program = std::move(InProgram);
		Builtins = InBuiltins;
		Stack.clear();
		Frames.clear();
		Globals.assign(Program->GlobalNames.size(), FValue::MakeUndefined());
		FFrame Main;
		Frames.push_back(Main);
		Status = EVMStatus::Ready;
		ErrorMessage.clear();
		ErrorLine = 0;
		CurrentLine = 0;
		TotalOps = 0;
	}

	bool FVM::Fail(const std::string& Message)
	{
		Status = EVMStatus::Error;
		ErrorMessage = Message;
		ErrorLine = CurrentLine;
		return false;
	}

	void FVM::SetYieldResult(const FValue& Value)
	{
		if (!Stack.empty())
		{
			Stack.back() = Value;
		}
	}

	static bool ToListIndex(const FValue& Key, size_t Size, size_t& OutIndex, std::string& OutError)
	{
		if (Key.Type != EValueType::Number || Key.NumberValue != std::floor(Key.NumberValue))
		{
			OutError = std::string("TypeError: indices must be whole numbers, not ") + Key.TypeName();
			return false;
		}
		long long Index = static_cast<long long>(Key.NumberValue);
		if (Index < 0)
		{
			Index += static_cast<long long>(Size);
		}
		if (Index < 0 || Index >= static_cast<long long>(Size))
		{
			OutError = "IndexError: index out of range";
			return false;
		}
		OutIndex = static_cast<size_t>(Index);
		return true;
	}

	bool FVM::DoIndex(const FValue& Container, const FValue& Key, FValue& Out)
	{
		std::string Err;
		size_t At = 0;
		if (Container.Type == EValueType::List)
		{
			if (!ToListIndex(Key, Container.ListValue->size(), At, Err))
			{
				return Fail(Err);
			}
			Out = (*Container.ListValue)[At];
			return true;
		}
		if (Container.Type == EValueType::String)
		{
			if (!ToListIndex(Key, Container.StringValue.size(), At, Err))
			{
				return Fail(Err);
			}
			Out = FValue::MakeString(std::string(1, Container.StringValue[At]));
			return true;
		}
		return Fail(std::string("TypeError: '") + Container.TypeName() + "' object is not subscriptable");
	}

	bool FVM::DoStoreIndex(const FValue& Container, const FValue& Key, const FValue& Value)
	{
		if (Container.Type != EValueType::List)
		{
			return Fail(std::string("TypeError: '") + Container.TypeName() + "' object does not support item assignment");
		}
		std::string Err;
		size_t At = 0;
		if (!ToListIndex(Key, Container.ListValue->size(), At, Err))
		{
			return Fail(Err);
		}
		(*Container.ListValue)[At] = Value;
		return true;
	}

	bool FVM::DoBinary(EBinaryOp Op, const FValue& Left, const FValue& Right, FValue& Out)
	{
		const auto AsNumber = [](const FValue& V, double& OutNumber)
		{
			if (V.Type == EValueType::Number) { OutNumber = V.NumberValue; return true; }
			if (V.Type == EValueType::Bool) { OutNumber = V.BoolValue ? 1.0 : 0.0; return true; }
			return false;
		};

		const auto TypeError = [&](const char* Symbol)
		{
			return Fail(std::string("TypeError: unsupported operand types for ") + Symbol + ": '" + Left.TypeName() + "' and '" + Right.TypeName() + "'");
		};

		double A = 0.0;
		double B = 0.0;
		const bool bNumbers = AsNumber(Left, A) && AsNumber(Right, B);

		switch (Op)
		{
		case EBinaryOp::Add:
			if (bNumbers) { Out = FValue::MakeNumber(A + B); return true; }
			if (Left.Type == EValueType::String && Right.Type == EValueType::String)
			{
				Out = FValue::MakeString(Left.StringValue + Right.StringValue);
				return true;
			}
			if (Left.Type == EValueType::List && Right.Type == EValueType::List)
			{
				if (Left.ListValue->size() + Right.ListValue->size() > MaxListSize)
				{
					return Fail("MemoryError: list is too big");
				}
				std::vector<FValue> Items(*Left.ListValue);
				Items.insert(Items.end(), Right.ListValue->begin(), Right.ListValue->end());
				Out = FValue::MakeList(std::move(Items));
				return true;
			}
			if (Left.Type == EValueType::String || Right.Type == EValueType::String)
			{
				return Fail("TypeError: can only join text with text (use str(x) to convert a number)");
			}
			return TypeError("+");
		case EBinaryOp::Sub:
			if (!bNumbers) return TypeError("-");
			Out = FValue::MakeNumber(A - B);
			return true;
		case EBinaryOp::Mul:
			if (!bNumbers) return TypeError("*");
			Out = FValue::MakeNumber(A * B);
			return true;
		case EBinaryOp::Div:
			if (!bNumbers) return TypeError("/");
			if (B == 0.0) return Fail("ZeroDivisionError: division by zero");
			Out = FValue::MakeNumber(A / B);
			return true;
		case EBinaryOp::FloorDiv:
			if (!bNumbers) return TypeError("//");
			if (B == 0.0) return Fail("ZeroDivisionError: division by zero");
			Out = FValue::MakeNumber(std::floor(A / B));
			return true;
		case EBinaryOp::Mod:
		{
			if (!bNumbers) return TypeError("%");
			if (B == 0.0) return Fail("ZeroDivisionError: modulo by zero");
			double R = std::fmod(A, B);
			if (R != 0.0 && ((R < 0.0) != (B < 0.0)))
			{
				R += B;
			}
			Out = FValue::MakeNumber(R);
			return true;
		}
		case EBinaryOp::Eq:
			Out = FValue::MakeBool(Left.Equals(Right));
			return true;
		case EBinaryOp::NotEq:
			Out = FValue::MakeBool(!Left.Equals(Right));
			return true;
		case EBinaryOp::Less:
		case EBinaryOp::LessEq:
		case EBinaryOp::Greater:
		case EBinaryOp::GreaterEq:
		{
			int Cmp = 0;
			if (bNumbers)
			{
				Cmp = A < B ? -1 : (A > B ? 1 : 0);
			}
			else if (Left.Type == EValueType::String && Right.Type == EValueType::String)
			{
				Cmp = Left.StringValue.compare(Right.StringValue);
			}
			else
			{
				return Fail(std::string("TypeError: can't compare '") + Left.TypeName() + "' with '" + Right.TypeName() + "'");
			}
			bool bResult = false;
			if (Op == EBinaryOp::Less) bResult = Cmp < 0;
			else if (Op == EBinaryOp::LessEq) bResult = Cmp <= 0;
			else if (Op == EBinaryOp::Greater) bResult = Cmp > 0;
			else bResult = Cmp >= 0;
			Out = FValue::MakeBool(bResult);
			return true;
		}
		case EBinaryOp::In:
		case EBinaryOp::NotIn:
		{
			bool bFound = false;
			if (Right.Type == EValueType::List)
			{
				for (const FValue& Item : *Right.ListValue)
				{
					if (Item.Equals(Left))
					{
						bFound = true;
						break;
					}
				}
			}
			else if (Right.Type == EValueType::String && Left.Type == EValueType::String)
			{
				bFound = Right.StringValue.find(Left.StringValue) != std::string::npos;
			}
			else
			{
				return Fail(std::string("TypeError: argument of type '") + Right.TypeName() + "' is not a list");
			}
			Out = FValue::MakeBool(Op == EBinaryOp::In ? bFound : !bFound);
			return true;
		}
		}
		return Fail("internal error: unknown operator");
	}

	bool FVM::DoCallMethod(const std::string& Name, int ArgCount)
	{
		const size_t ObjIndex = Stack.size() - static_cast<size_t>(ArgCount) - 1;
		const FValue Object = Stack[ObjIndex];
		std::vector<FValue> Args(Stack.begin() + static_cast<std::ptrdiff_t>(ObjIndex + 1), Stack.end());
		Stack.resize(ObjIndex);

		if (Object.Type != EValueType::List)
		{
			return Fail(std::string("AttributeError: '") + Object.TypeName() + "' object has no method '" + Name + "'");
		}

		std::vector<FValue>& Items = *Object.ListValue;
		if (Name == "append")
		{
			if (Args.size() != 1) return Fail("TypeError: append() takes exactly 1 argument");
			if (Items.size() >= MaxListSize) return Fail("MemoryError: list is too big");
			Items.push_back(Args[0]);
			Stack.push_back(FValue::MakeNone());
			return true;
		}
		if (Name == "pop")
		{
			if (Args.size() > 1) return Fail("TypeError: pop() takes at most 1 argument");
			if (Items.empty()) return Fail("IndexError: pop from empty list");
			size_t At = Items.size() - 1;
			if (Args.size() == 1)
			{
				std::string Err;
				if (!ToListIndex(Args[0], Items.size(), At, Err)) return Fail(Err);
			}
			const FValue Removed = Items[At];
			Items.erase(Items.begin() + static_cast<std::ptrdiff_t>(At));
			Stack.push_back(Removed);
			return true;
		}
		if (Name == "insert")
		{
			if (Args.size() != 2) return Fail("TypeError: insert() takes exactly 2 arguments");
			if (Args[0].Type != EValueType::Number) return Fail("TypeError: insert() index must be a number");
			long long At = static_cast<long long>(Args[0].NumberValue);
			const long long Size = static_cast<long long>(Items.size());
			if (At < 0) At += Size;
			At = At < 0 ? 0 : (At > Size ? Size : At);
			Items.insert(Items.begin() + static_cast<std::ptrdiff_t>(At), Args[1]);
			Stack.push_back(FValue::MakeNone());
			return true;
		}
		if (Name == "index")
		{
			if (Args.size() != 1) return Fail("TypeError: index() takes exactly 1 argument");
			for (size_t i = 0; i < Items.size(); ++i)
			{
				if (Items[i].Equals(Args[0]))
				{
					Stack.push_back(FValue::MakeNumber(static_cast<double>(i)));
					return true;
				}
			}
			return Fail("ValueError: value is not in list");
		}
		return Fail("AttributeError: 'list' object has no method '" + Name + "'");
	}

	EVMStatus FVM::Run(int MaxOps)
	{
		if (!Program || IsHalted())
		{
			return Status;
		}
		Status = EVMStatus::Ready;

		int Ops = 0;
		while (Ops < MaxOps)
		{
			FFrame& Frame = Frames.back();
			const FFunctionProto& Proto = Program->Functions[static_cast<size_t>(Frame.Function)];
			if (Frame.Ip < 0 || Frame.Ip >= static_cast<int>(Proto.Code.size()))
			{
				Fail("internal error: instruction pointer out of range");
				return Status;
			}
			const FInstr Instr = Proto.Code[static_cast<size_t>(Frame.Ip)];
			++Frame.Ip;
			CurrentLine = Instr.Line;
			++Ops;
			++TotalOps;

			if (Stack.size() > MaxStackSize)
			{
				Fail("MemoryError: stack overflow");
				return Status;
			}

			switch (Instr.Op)
			{
			case EOp::Const:
				Stack.push_back(Program->Constants[static_cast<size_t>(Instr.A)]);
				break;

			case EOp::LoadLocal:
				Stack.push_back(Stack[static_cast<size_t>(Frame.Base + Instr.A)]);
				break;

			case EOp::StoreLocal:
				Stack[static_cast<size_t>(Frame.Base + Instr.A)] = Stack.back();
				Stack.pop_back();
				break;

			case EOp::LoadGlobal:
			{
				const FValue& Value = Globals[static_cast<size_t>(Instr.A)];
				if (Value.Type == EValueType::Undefined)
				{
					Fail("NameError: name '" + Program->GlobalNames[static_cast<size_t>(Instr.A)] + "' is not defined");
					return Status;
				}
				Stack.push_back(Value);
				break;
			}

			case EOp::StoreGlobal:
				Globals[static_cast<size_t>(Instr.A)] = Stack.back();
				Stack.pop_back();
				break;

			case EOp::LoadBuiltin:
			{
				const FBuiltin& Builtin = Builtins->Get(Instr.A);
				Stack.push_back(Builtin.Fn ? FValue::MakeBuiltin(Instr.A) : Builtin.Constant);
				break;
			}

			case EOp::Pop:
				Stack.pop_back();
				break;

			case EOp::Dup2:
			{
				const size_t Size = Stack.size();
				const FValue First = Stack[Size - 2];
				const FValue Second = Stack[Size - 1];
				Stack.push_back(First);
				Stack.push_back(Second);
				break;
			}

			case EOp::Binary:
			{
				const FValue Right = Stack.back();
				Stack.pop_back();
				const FValue Left = Stack.back();
				Stack.pop_back();
				FValue Result;
				if (!DoBinary(static_cast<EBinaryOp>(Instr.A), Left, Right, Result))
				{
					return Status;
				}
				Stack.push_back(Result);
				break;
			}

			case EOp::Negate:
			{
				FValue& Top = Stack.back();
				if (Top.Type != EValueType::Number)
				{
					Fail(std::string("TypeError: bad operand type for unary -: '") + Top.TypeName() + "'");
					return Status;
				}
				Top.NumberValue = -Top.NumberValue;
				break;
			}

			case EOp::Not:
				Stack.back() = FValue::MakeBool(!Stack.back().IsTruthy());
				break;

			case EOp::Jump:
				Frame.Ip = Instr.A;
				break;

			case EOp::JumpIfFalse:
			{
				const bool bTruthy = Stack.back().IsTruthy();
				Stack.pop_back();
				if (!bTruthy)
				{
					Frame.Ip = Instr.A;
				}
				break;
			}

			case EOp::JumpIfFalseOrPop:
				if (!Stack.back().IsTruthy())
				{
					Frame.Ip = Instr.A;
				}
				else
				{
					Stack.pop_back();
				}
				break;

			case EOp::JumpIfTrueOrPop:
				if (Stack.back().IsTruthy())
				{
					Frame.Ip = Instr.A;
				}
				else
				{
					Stack.pop_back();
				}
				break;

			case EOp::MakeList:
			{
				const size_t Count = static_cast<size_t>(Instr.A);
				std::vector<FValue> Items(Stack.end() - static_cast<std::ptrdiff_t>(Count), Stack.end());
				Stack.resize(Stack.size() - Count);
				Stack.push_back(FValue::MakeList(std::move(Items)));
				break;
			}

			case EOp::Index:
			{
				const FValue Key = Stack.back();
				Stack.pop_back();
				const FValue Container = Stack.back();
				Stack.pop_back();
				FValue Result;
				if (!DoIndex(Container, Key, Result))
				{
					return Status;
				}
				Stack.push_back(Result);
				break;
			}

			case EOp::StoreIndex:
			{
				const FValue Value = Stack.back();
				Stack.pop_back();
				const FValue Key = Stack.back();
				Stack.pop_back();
				const FValue Container = Stack.back();
				Stack.pop_back();
				if (!DoStoreIndex(Container, Key, Value))
				{
					return Status;
				}
				break;
			}

			case EOp::Call:
			{
				const int ArgCount = Instr.A;
				const size_t CalleeIndex = Stack.size() - static_cast<size_t>(ArgCount) - 1;
				const FValue Callee = Stack[CalleeIndex];

				if (Callee.Type == EValueType::Function)
				{
					const FFunctionProto& Target = Program->Functions[static_cast<size_t>(Callee.Index)];
					if (ArgCount != Target.NumParams)
					{
						Fail("TypeError: " + Target.Name + "() takes " + std::to_string(Target.NumParams) +
							" argument(s) but " + std::to_string(ArgCount) + " were given");
						return Status;
					}
					if (Frames.size() >= MaxCallDepth)
					{
						Fail("RecursionError: maximum recursion depth exceeded");
						return Status;
					}
					FFrame NewFrame;
					NewFrame.Function = Callee.Index;
					NewFrame.Ip = 0;
					NewFrame.Base = static_cast<int>(CalleeIndex) + 1;
					Stack.resize(static_cast<size_t>(NewFrame.Base + Target.NumLocals));
					Frames.push_back(NewFrame); // invalidates Frame
					break;
				}

				if (Callee.Type == EValueType::Builtin)
				{
					const FBuiltin& Builtin = Builtins->Get(Callee.Index);
					if (Builtin.bLocked)
					{
						Fail(Builtin.LockedMessage.empty() ? "Locked: " + Builtin.Name + "() is not unlocked yet" : Builtin.LockedMessage);
						return Status;
					}
					if (ArgCount < Builtin.MinArgs || ArgCount > Builtin.MaxArgs)
					{
						std::string Expected = std::to_string(Builtin.MinArgs);
						if (Builtin.MaxArgs != Builtin.MinArgs)
						{
							Expected += " to " + std::to_string(Builtin.MaxArgs);
						}
						Fail("TypeError: " + Builtin.Name + "() takes " + Expected + " argument(s) but " + std::to_string(ArgCount) + " were given");
						return Status;
					}
					const std::vector<FValue> Args(Stack.begin() + static_cast<std::ptrdiff_t>(CalleeIndex + 1), Stack.end());
					FValue Result;
					std::string CallError;
					const ECallResult CallResult = Builtin.Fn(Args, Result, CallError);
					if (CallResult == ECallResult::Error)
					{
						Fail(CallError);
						return Status;
					}
					Stack.resize(CalleeIndex);
					Stack.push_back(Result);
					if (CallResult == ECallResult::Suspend)
					{
						Status = EVMStatus::Yielded;
						return Status;
					}
					break;
				}

				Fail(std::string("TypeError: '") + Callee.TypeName() + "' object is not callable");
				return Status;
			}

			case EOp::CallMethod:
				if (!DoCallMethod(Program->Constants[static_cast<size_t>(Instr.A)].StringValue, Instr.B))
				{
					return Status;
				}
				break;

			case EOp::Return:
			{
				const FValue Result = Stack.back();
				const int Base = Frame.Base;
				Frames.pop_back(); // invalidates Frame
				if (Frames.empty())
				{
					Status = EVMStatus::Finished;
					return Status;
				}
				Stack.resize(static_cast<size_t>(Base - 1)); // drops locals and the callee
				Stack.push_back(Result);
				break;
			}

			case EOp::GetIter:
			{
				const FValue Iterable = Stack.back();
				Stack.pop_back();
				if (Iterable.Type != EValueType::List)
				{
					Fail(std::string("TypeError: '") + Iterable.TypeName() + "' object is not iterable (use range(n) to loop n times)");
					return Status;
				}
				// Iterate over a snapshot so modifying the list inside the loop is safe.
				Stack.push_back(FValue::MakeList(*Iterable.ListValue));
				Stack.push_back(FValue::MakeNumber(0.0));
				break;
			}

			case EOp::ForIter:
			{
				const size_t Size = Stack.size();
				const size_t Position = static_cast<size_t>(Stack[Size - 1].NumberValue);
				const std::vector<FValue>& Items = *Stack[Size - 2].ListValue;
				if (Position < Items.size())
				{
					const FValue Item = Items[Position];
					Stack[Size - 1].NumberValue += 1.0;
					Stack.push_back(Item);
				}
				else
				{
					Stack.pop_back();
					Stack.pop_back();
					Frame.Ip = Instr.A;
				}
				break;
			}

			case EOp::Halt:
				Status = EVMStatus::Finished;
				return Status;
			}
		}

		Status = EVMStatus::OutOfOps;
		return Status;
	}
}
