// FightScript: a tiny Python-like language compiled to bytecode and executed by a
// step-able VM. It is plain C++ (no engine dependencies) so it can be unit tested
// outside Unreal (see CodeArena/Tests).
//
// The VM never blocks: the host calls Run(MaxOps) every game tick and the VM
// returns as soon as the script calls an "action" builtin (move, attack, ...),
// runs out of operations for this tick, finishes or hits an error.

#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace FightScript
{
	enum class EValueType : uint8_t
	{
		Undefined, // internal: global slot that was never assigned
		None,
		Bool,
		Number,
		String,
		List,
		Function,
		Builtin
	};

	struct FValue;
	using FListPtr = std::shared_ptr<std::vector<FValue>>;

	struct FValue
	{
		EValueType Type = EValueType::None;
		bool BoolValue = false;
		double NumberValue = 0.0;
		std::string StringValue;
		FListPtr ListValue;
		int Index = -1; // function / builtin index

		static FValue MakeUndefined();
		static FValue MakeNone();
		static FValue MakeBool(bool bValue);
		static FValue MakeNumber(double Value);
		static FValue MakeString(const std::string& Value);
		static FValue MakeList(std::vector<FValue> Items);
		static FValue MakeFunction(int FunctionIndex);
		static FValue MakeBuiltin(int BuiltinIndex);

		bool IsTruthy() const;
		bool Equals(const FValue& Other) const;
		std::string ToDisplayString() const;
		const char* TypeName() const;
	};

	std::string FormatNumber(double Value);

	enum class ECallResult : uint8_t
	{
		Ok,    // result is ready, keep running
		Suspend, // result is a placeholder, pause the VM (game action)
		Error
	};

	using FBuiltinFn = std::function<ECallResult(const std::vector<FValue>& Args, FValue& OutResult, std::string& OutError)>;

	struct FBuiltin
	{
		std::string Name;
		int MinArgs = 0;
		int MaxArgs = 0;
		FBuiltinFn Fn;       // empty => this builtin is a constant
		FValue Constant;
		bool bLocked = false;
		std::string LockedMessage;
	};

	class FBuiltinTable
	{
	public:
		int AddFunction(const std::string& Name, int MinArgs, int MaxArgs, FBuiltinFn Fn);
		int AddConstant(const std::string& Name, const FValue& Value);
		int Find(const std::string& Name) const;
		void SetLocked(const std::string& Name, bool bLocked, const std::string& Message);

		FBuiltin& Get(int Index) { return Items[static_cast<size_t>(Index)]; }
		const FBuiltin& Get(int Index) const { return Items[static_cast<size_t>(Index)]; }
		int Num() const { return static_cast<int>(Items.size()); }

	private:
		std::vector<FBuiltin> Items;
		std::unordered_map<std::string, int> ByName;
	};

	using FPrintFn = std::function<void(const std::string&)>;

	// Adds print, range, len, str, int, abs, min, max, random, randint.
	// RandomState is used by random()/randint() and must outlive the table.
	void AddStandardLibrary(FBuiltinTable& Table, FPrintFn Print, uint32_t* RandomState);

	enum class EOp : uint8_t
	{
		Const,
		LoadLocal,
		StoreLocal,
		LoadGlobal,
		StoreGlobal,
		LoadBuiltin,
		Pop,
		Dup2,
		Binary,
		Negate,
		Not,
		Jump,
		JumpIfFalse,
		JumpIfFalseOrPop,
		JumpIfTrueOrPop,
		MakeList,
		Index,
		StoreIndex,
		Call,
		CallMethod,
		Return,
		GetIter,
		ForIter,
		Halt
	};

	enum class EBinaryOp : uint8_t
	{
		Add, Sub, Mul, Div, FloorDiv, Mod,
		Eq, NotEq, Less, LessEq, Greater, GreaterEq,
		In, NotIn
	};

	struct FInstr
	{
		EOp Op = EOp::Halt;
		int A = 0;
		int B = 0;
		int Line = 0;
	};

	struct FFunctionProto
	{
		std::string Name;
		int NumParams = 0;
		int NumLocals = 0;
		std::vector<FInstr> Code;
	};

	struct FProgram
	{
		std::vector<FFunctionProto> Functions; // [0] is the module body
		std::vector<FValue> Constants;
		std::vector<std::string> GlobalNames;
	};

	// Language features that can be locked behind the in-game shop.
	struct FCompileOptions
	{
		bool bAllowFor = true;
		bool bAllowDef = true;
		bool bAllowLists = true;
	};

	struct FCompileError
	{
		int Line = 0;
		std::string Message;
	};

	bool Compile(const std::string& Source, const FBuiltinTable& Builtins, const FCompileOptions& Options,
		FProgram& OutProgram, FCompileError& OutError);

	enum class EVMStatus : uint8_t
	{
		Ready,
		Yielded,  // script called an action builtin
		OutOfOps, // budget for this tick was spent
		Finished,
		Error
	};

	class FVM
	{
	public:
		void Load(std::shared_ptr<const FProgram> InProgram, FBuiltinTable* InBuiltins);

		// Executes at most MaxOps instructions.
		EVMStatus Run(int MaxOps);

		// Replaces the return value of the last yielded builtin call.
		void SetYieldResult(const FValue& Value);

		EVMStatus GetStatus() const { return Status; }
		bool IsHalted() const { return Status == EVMStatus::Finished || Status == EVMStatus::Error; }
		const std::string& GetError() const { return ErrorMessage; }
		int GetErrorLine() const { return ErrorLine; }
		int GetCurrentLine() const { return CurrentLine; }
		int64_t GetTotalOps() const { return TotalOps; }

	private:
		struct FFrame
		{
			int Function = 0;
			int Ip = 0;
			int Base = 0;
		};

		bool Fail(const std::string& Message);
		bool DoBinary(EBinaryOp Op, const FValue& Left, const FValue& Right, FValue& Out);
		bool DoIndex(const FValue& Container, const FValue& Key, FValue& Out);
		bool DoStoreIndex(const FValue& Container, const FValue& Key, const FValue& Value);
		bool DoCallMethod(const std::string& Name, int ArgCount);

		std::shared_ptr<const FProgram> Program;
		FBuiltinTable* Builtins = nullptr;
		std::vector<FValue> Stack;
		std::vector<FFrame> Frames;
		std::vector<FValue> Globals;
		EVMStatus Status = EVMStatus::Finished;
		std::string ErrorMessage;
		int ErrorLine = 0;
		int CurrentLine = 0;
		int64_t TotalOps = 0;
	};
}
