#pragma once

namespace CSX::RenderMap::DevBenchBridge
{
	/** Register the optional bounded diagnostic adapter when DevBench is present. */
	void Install();
	/** Whether this process successfully published the adapter's tool. */
	bool IsRegistered();
}
