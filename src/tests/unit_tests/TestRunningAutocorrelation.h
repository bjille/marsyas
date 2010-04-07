#include <cxxtest/TestSuite.h>

#include "MarSystemManager.h"

// The class that you want to test, for example "WekaSource"
#include "RunningAutocorrelation.h"

using namespace std;
using namespace Marsyas;

/**
 * Sum of the integers from 0 to a given n (including).
 */
int sum_of_ints(int end) {
	return (end < 0) ? 0 : end * (end + 1) / 2;
}

/**
 * Sum of integers from given begin to end (including).
 */
int sum_of_ints(int n_begin, int n_end) {
	return (n_end < n_begin) ? 0 : sum_of_ints(n_end)
			- sum_of_ints(n_begin - 1);
}

/**
 * Sum of the squares from 0 to the given n (including).
 */
int sum_of_squares(int n) {
	return (n < 0) ? 0 : n * (n + 1) * (2 * n + 1) / 6;
}

/**
 * Sum of squares from given begin to end (including).
 */
int sum_of_squares(int n_begin, int n_end) {
	return (n_end < n_begin) ? 0 : sum_of_squares(n_end) - sum_of_squares(
			n_begin - 1);
}

/**
 * Calculate the autocorrelation function of
 *   x[n] = a * n + b  for n>=0
 *        = 0          for n < 0
 * The calculation for lag k (up to N samples):
 *   Rxx[k] = sum_{n=k}^{N} (a * n + b) * (a * n + b - k))
 *          = a^2 * sum_{n=k}^{N} n^2
 *          	+ (2ab - ak) * sum_{n=k}^{N} n
 *              + (b^2 - bk) * sum_{n=k}^{N} 1
 */
int autocorrelation_of_anplusb(int a, int b, int lag, int N) {
	return a * a * sum_of_squares(lag, N) + (2 * a * b - a * lag)
			* sum_of_ints(lag, N) + (b * b - b * lag) * (N + 1 - lag);
}

class RunningAutocorrelation_runner: public CxxTest::TestSuite {
public:
	realvec in, out;
	MarSystemManager mng;
	RunningAutocorrelation *rac;

	void setUp() {
		rac = new RunningAutocorrelation("rac");
	}

	/**
	 * Set up the internal RunningAutocorrelation MarSystem with the input
	 * flow parameters and maxLag and check if the output flow
	 * parameters are set accordingly.
	 * Also allocate the input and output realvecs.
	 */
	void set_flow(mrs_natural inObservations, mrs_natural inSamples,
			mrs_natural maxLag) {
		rac->updctrl("mrs_natural/inObservations", inObservations);
		rac->updctrl("mrs_natural/inSamples", inSamples);
		rac->updctrl("mrs_natural/maxLag", maxLag);
		TS_ASSERT_EQUALS(rac->getControl("mrs_natural/onObservations")->to<mrs_natural>(), inObservations);
		TS_ASSERT_EQUALS(rac->getControl("mrs_natural/onSamples")->to<mrs_natural>(), maxLag + 1);
		// Allocate the input and output vectors.
		in.create(inObservations, inSamples);
		out.create(inObservations, maxLag + 1);
	}

	void test_output_flow(void) {
		// Try some flow settings
		set_flow(1, 100, 10);
		set_flow(10, 10, 7);
		set_flow(10, 4, 10);
		set_flow(4, 10, 10);
	}

	/**
	 * Test the processing of one slice (differently put: the previous slices
	 * were all zero).
	 * The input is a single rows [0, 1, 2, ..., 9]:
	 *    x[0, n] = n     for n=0..9
	 * The output for lag=k should be
	 *    Rxx[k] = sum_{n=k}^{9} n * (n - k)
	 *           = sum_{n=k}^{9} n^2 - k * sum_{n=k}^9 n
	 */
	void test_one_slice_single_row(void) {
		mrs_natural inObservations = 1;
		mrs_natural inSamples = 10;
		mrs_natural maxLag = 6;
		set_flow(inObservations, inSamples, maxLag);

		// Create input
		for (mrs_natural t = 0; t < inSamples; t++) {
			in(0, t) = t;
		}

		// Process.
		rac->myProcess(in, out);

		// Check output.
		mrs_natural expected;
		for (mrs_natural lag = 0; lag <= maxLag; lag++) {
			expected = autocorrelation_of_anplusb(1, 0, lag, inSamples - 1);
			TS_ASSERT_EQUALS(out(0, lag), expected);
		}

	}

	/**
	 * Test the processing of one slice with several rows
	 *    x[r, n] = r + n  for r=0..5 and n=0..9
	 *    e.g. row r: [r, r + 1, ..., r + 9]
	 * The output for lag=k should be for row r:
	 *     Rxx[k] = sum_{n=k}^{9} (r + n) * (r + n - k)
	 *            = sum_{n=k}^{9} n^2 + (2r - k) * sum_{n=k}^9 n + (r^2-rk) * sum_{n=k}^9 1
	 */
	void test_one_slice_multirow(void) {
		mrs_natural inObservations = 5;
		mrs_natural inSamples = 10;
		mrs_natural maxLag = 6;
		set_flow(inObservations, inSamples, maxLag);

		// Create input.
		for (mrs_natural r = 0; r < inObservations; r++) {
			for (mrs_natural t = 0; t < inSamples; t++) {
				in(r, t) = r + t;
			}
		}

		// Process.
		rac->myProcess(in, out);

		// Check output.
		mrs_natural expected;
		for (mrs_natural r = 0; r < inObservations; r++) {
			for (mrs_natural lag = 0; lag <= maxLag; lag++) {
				expected = autocorrelation_of_anplusb(1, r, lag, inSamples - 1);
				TS_ASSERT_EQUALS(out(r,  lag), expected);
			}
		}
	}

	/**
	 * Test the processing of two successive slices (single row).
	 * First slice: [0, 1, 2, ..., 9]:
	 *    x[0, n] = n     for n=0..9
	 * Second slice: [10, 11, 12, ..., 19]:
	 *    x[0,n] = 10 + n  for n=0..9
	 * The output for second slice for lag=k should be
	 *    Rxx[k] = sum_{n=k}^{19} n * (n - k)
	 *           = sum_{n=k}^{19} n^2 - k * sum_{n=k}^19 n
	 */
	void test_two_slices_single_row(void) {
		mrs_natural inObservations = 1;
		mrs_natural inSamples = 10;
		mrs_natural maxLag = 6;
		set_flow(inObservations, inSamples, maxLag);

		// Create input.
		// First slice.
		for (mrs_natural t = 0; t < inSamples; t++) {
			in(0, t) = t;
		}
		// Process.
		rac->myProcess(in, out);

		// Second slice
		for (mrs_natural t = 0; t < inSamples; t++) {
			in(0, t) = inSamples + t;
		}
		// Process.
		rac->myProcess(in, out);

		// Check output.
		mrs_natural expected;
		for (mrs_natural lag = 0; lag <= maxLag; lag++) {
			expected = autocorrelation_of_anplusb(1, 0, lag, 2 * inSamples - 1);
			TS_ASSERT_EQUALS(out(0, lag), expected);
		}
	}

	/**
	 * Test the processing of several slices (10 samples per slice)
	 * coming from a multiple channel signal:
	 *    x[r, n] = r + n  for r=0..5 and n=0..\infty
	 *    e.g. row r: [r, r+1, r+2, ...]
	 * The output after S slices for lag=k should be for row r:
	 *     Rxx[k] = sum_{n=k}^{10 S - 1} (r + n) * (r + n - k))
	 *            = sum_{n=k}^{10 S - 1} n^2
	 *                + (2r - k) * sum_{n=k}^{10 S - 1} n
	 *                + (r^2-rk) * sum_{n=k}^{10 S - 1} 1
	 */
	void test_multiple_slices_multirow(void) {
		mrs_natural inObservations = 5;
		mrs_natural inSamples = 10;
		mrs_natural maxLag = 6;
		mrs_natural S = 7;
		set_flow(inObservations, inSamples, maxLag);

		// Create input.
		// Feed with multiple multirow slices.
		for (mrs_natural s = 0; s < S; s++) {
			for (mrs_natural r = 0; r < inObservations; r++) {
				for (mrs_natural t = 0; t < inSamples; t++) {
					in(r, t) = r + (s * inSamples + t);
				}
			}
			// Process.
			rac->myProcess(in, out);
		}

		// Check output.
		mrs_natural expected;
		for (mrs_natural r = 0; r < inObservations; r++) {
			for (mrs_natural lag = 0; lag <= maxLag; lag++) {
				expected = autocorrelation_of_anplusb(1, r, lag, S * inSamples
						- 1);
				TS_ASSERT_EQUALS(out(r, lag), expected);
			}
		}
	}

	/**
	 * Test with a maxLag larger than the slice size
	 */
	void test_maxlag_larger_than_slice_size(void) {
		mrs_natural inObservations = 5;
		mrs_natural inSamples = 7;
		mrs_natural maxLag = 33;
		mrs_natural S = 7;
		set_flow(inObservations, inSamples, maxLag);

		// Create input.
		// Feed with multiple multirow slices.
		for (mrs_natural s = 0; s < S; s++) {
			for (mrs_natural r = 0; r < inObservations; r++) {
				for (mrs_natural t = 0; t < inSamples; t++) {
					in(r, t) = r + (s * inSamples + t);
				}
			}
			// Process.
			rac->myProcess(in, out);
		}

		// Check output.
		mrs_natural expected;
		for (mrs_natural r = 0; r < inObservations; r++) {
			for (mrs_natural lag = 0; lag <= maxLag; lag++) {
				expected = autocorrelation_of_anplusb(1, r, lag, S * inSamples
						- 1);
				TS_ASSERT_EQUALS(out(r, lag), expected);
			}
		}
	}

	/**
	 * Test the observation names
	 */
	void test_observation_names() {
		set_flow(2, 5, 3);
		rac->updctrl("mrs_string/inObsNames", "foo,bar,");
		mrs_string onObsNames = rac->getctrl("mrs_string/onObsNames")->to<
				mrs_string> ();
		TS_ASSERT_EQUALS(onObsNames, "foo,bar,")
	}

};

