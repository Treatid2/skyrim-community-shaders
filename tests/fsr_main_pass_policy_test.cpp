#include "Features/Upscaling/FSRMainPassPolicy.h"

namespace
{
	using FSRMainPassPolicy::ClassifyResetBlock;
	using FSRMainPassPolicy::RequiresCurrentInputFallback;
	using FSRMainPassPolicy::Result;

	static_assert(ClassifyResetBlock(true, false, false) == Result::Ready);
	static_assert(ClassifyResetBlock(false, false, false) == Result::Deferred);
	static_assert(ClassifyResetBlock(false, true, false) == Result::Failed);
	static_assert(ClassifyResetBlock(false, false, true) == Result::Failed);
	static_assert(ClassifyResetBlock(false, true, true) == Result::Failed);

	static_assert(!RequiresCurrentInputFallback(true, Result::Ready));
	static_assert(RequiresCurrentInputFallback(true, Result::Deferred));
	static_assert(RequiresCurrentInputFallback(true, Result::Failed));
	static_assert(!RequiresCurrentInputFallback(false, Result::Ready));
	static_assert(!RequiresCurrentInputFallback(false, Result::Deferred));
	static_assert(!RequiresCurrentInputFallback(false, Result::Failed));
}

int main() {}
