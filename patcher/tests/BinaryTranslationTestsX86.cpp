#include <patcher/binary_translation.h>

#include "helpers.h"

#include <ut/assert.h>
#include <ut/test.h>

using namespace std;

namespace micro_profiler
{
	namespace tests
	{
		begin_test_suite( BinaryTranslationTestsX86 )
			test( RelativeOutsideJumpsAreTranslatedBasedOnTheirTargetAddress )
			{
				// INIT
				byte instructions1[] = {
					0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
					0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
					0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
					0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
					0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
					0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
					0xE9, 0xF0, 0xFF, 0xFF, 0xFF, 0xE9, 0x00, 0x00, 0x10, 0x00,
				};

				// ACT
				move_function(instructions1 + 1, instructions1 + 48, const_byte_range(instructions1 + 48, 10));

				// ASSERT
				byte reference1[] = {
					0xE9, 0x1F, 0x00, 0x00, 0x00, 0xE9, 0x2F, 0x00, 0x10, 0x00,
				};

				assert_is_true(equal(reference1, array_end(reference1), instructions1 + 1));

				// ACT
				move_function(instructions1 + 17, instructions1 + 48, const_byte_range(instructions1 + 48, 10));

				// ASSERT
				byte reference2[] = {
					0xE9, 0x0F, 0x00, 0x00, 0x00, 0xE9, 0x1F, 0x00, 0x10, 0x00,
				};

				assert_is_true(equal(reference2, array_end(reference2), instructions1 + 17));

				// INIT
				byte instructions2[0x1000] = {
					0xE9, 0x21, 0x10, 0x00, 0x10, 0xE9, 0x00, 0xFF, 0xFF, 0xFF, 0xE9, 0xFF, 0x00, 0x00, 0x00,
				};
		
				// ACT
				move_function(instructions2 + 0x0F12, instructions2, const_byte_range(instructions2, 15));

				// ASSERT
				byte reference3[] = {
					0xE9, 0x0F, 0x01, 0x00, 0x10, 0xE9, 0xEE, 0xEF, 0xFF, 0xFF, 0xE9, 0xED, 0xF1, 0xFF, 0xFF,
				};

				assert_is_true(equal(reference3, array_end(reference3), instructions2 + 0x0F12));
			}


			test( RelativeOutsideCallsAreTranslatedBasedOnTheirTargetAddress )
			{
				// INIT
				byte instructions1[] = {
					0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
					0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
					0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
					0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
					0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
					0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
					0xE8, 0xF0, 0xFF, 0xFF, 0xFF, 0xE8, 0x00, 0x00, 0x10, 0x00, 0xE8, 0x00, 0x00, 0x00, 0x00,
				};

				// ACT
				move_function(instructions1 + 1, instructions1 + 48, const_byte_range(instructions1 + 48, 15));

				// ASSERT
				byte reference1[] = {
					0xE8, 0x1F, 0x00, 0x00, 0x00, 0xE8, 0x2F, 0x00, 0x10, 0x00, 0xE8, 0x2F, 0x00, 0x00, 0x00,
				};

				assert_is_true(equal(reference1, array_end(reference1), instructions1 + 1));

				// ACT
				move_function(instructions1 + 17, instructions1 + 48, const_byte_range(instructions1 + 48, 15));

				// ASSERT
				byte reference2[] = {
					0xE8, 0x0F, 0x00, 0x00, 0x00, 0xE8, 0x1F, 0x00, 0x10, 0x00, 0xE8, 0x1F, 0x00, 0x00, 0x00,
				};

				assert_is_true(equal(reference2, array_end(reference2), instructions1 + 17));

				// INIT
				byte instructions2[0x1000] = {
					0xE8, 0x21, 0x10, 0x00, 0x10, 0xE8, 0x00, 0xFF, 0xFF, 0xFF, 0xE8, 0xFF, 0x00, 0x00, 0x00,
					0xE8, 0x00, 0x00, 0x00, 0x00,
				};
		
				// ACT
				move_function(instructions2 + 0x0F11, instructions2, const_byte_range(instructions2, 20));

				// ASSERT
				byte reference3[] = {
					0xE8, 0x10, 0x01, 0x00, 0x10, 0xE8, 0xEF, 0xEF, 0xFF, 0xFF, 0xE8, 0xEE, 0xF1, 0xFF, 0xFF,
					0xE8, 0xEF, 0xF0, 0xFF, 0xFF,
				};

				assert_is_true(equal(reference3, array_end(reference3), instructions2 + 0x0F11));
			}


			test( RelativeInnerJumpsAreNotTranslatedWhenMoved )
			{
				// INIT
				byte instructions[0x100] = {
					0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
					0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
					0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
					0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
					0xE9, 0x05, 0x00, 0x00, 0x00, 0xE9, 0x00, 0x00, 0x00, 0x00, 0xE9, 0xF1, 0xFF, 0xFF, 0xFF,
					0xE9, 0xF6, 0xFF, 0xFF, 0xFF,
				};

				// ACT
				move_function(instructions, instructions + 0xE1, const_byte_range(instructions + 0x20, 20));
				move_function(instructions + 0x40, instructions + 0x23, const_byte_range(instructions + 0x20, 20));

				// ASSERT
				byte reference[] = {
					0xE9, 0x05, 0x00, 0x00, 0x00, 0xE9, 0x00, 0x00, 0x00, 0x00, 0xE9, 0xF1, 0xFF, 0xFF, 0xFF,
					0xE9, 0xF6, 0xFF, 0xFF, 0xFF
				};

				assert_is_true(equal(reference, array_end(reference), instructions));
				assert_is_true(equal(reference, array_end(reference), instructions + 0x40));
			}


			test( RelativeInnerCallsAreNotTranslatedWhenMoved )
			{
				// INIT
				byte instructions[0x100] = {
					0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
					0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
					0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
					0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
					0xE8, 0x05, 0x00, 0x00, 0x00, 0xE8, 0x00, 0x00, 0x00, 0x00, 0xE8, 0xF6, 0xFF, 0xFF, 0xFF,
					0xE8, 0xF6, 0xFF, 0xFF, 0xFF,
				};

				// ACT
				move_function(instructions, instructions + 0x01, const_byte_range(instructions + 0x20, 20));
				move_function(instructions + 0x40, instructions + 0x23, const_byte_range(instructions + 0x20, 20));

				// ASSERT
				byte reference[] = {
					0xE8, 0x05, 0x00, 0x00, 0x00, 0xE8, 0x00, 0x00, 0x00, 0x00, 0xE8, 0xF6, 0xFF, 0xFF, 0xFF,
					0xE8, 0xF6, 0xFF, 0xFF, 0xFF,
				};

				assert_is_true(equal(reference, array_end(reference), instructions));
				assert_is_true(equal(reference, array_end(reference), instructions + 0x40));
			}


			test( InstructionMixWithExternalJumpsAndCallsIsTranslatedToTargetAddress )
			{
				// INIT
				byte instructions[0x400] = {
					0xE8, 0xD4, 0x54, 0x02, 0x00,			// call memset
					0x83, 0xC4, 0x0C,							// add esp, 0Ch
					0x6A, 0x0F,									// push 0Fh
					0x8D, 0x8D, 0xB4, 0xEF, 0xFF, 0xFF,	// lea ecx, [instructions2]
					0x51,											// push ecx
					0x8D, 0x95, 0xC5, 0xFE, 0xFF, 0xFF,	// lea edx, [ebp-13Bh]
					0x52,											// push edx
					0xE8, 0x8C, 0x98, 0x01, 0x00,			// call micro_profiler::move_function
					0x83, 0xC4, 0x0C,							// add esp, 0Ch
					0xE9, 0x12, 0x34, 0x56, 0x78,			// jmp somewhere...
				};
				byte *translated = instructions + 0x0123;

				// ACT
				move_function(translated, instructions, const_byte_range(instructions, 0x0025));

				// ASSERT
				byte reference1[] = {
					0xE8, 0xB1, 0x53, 0x02, 0x00,
					0x83, 0xC4, 0x0C,
					0x6A, 0x0F,
					0x8D, 0x8D, 0xB4, 0xEF, 0xFF, 0xFF,
					0x51,
					0x8D, 0x95, 0xC5, 0xFE, 0xFF, 0xFF,
					0x52,
					0xE8, 0x69, 0x97, 0x01, 0x00,
					0x83, 0xC4, 0x0C,
					0xE9, 0xEF, 0x32, 0x56, 0x78,
				};

				assert_is_true(equal(reference1, array_end(reference1), translated));

				// ACT
				move_function(translated, instructions + 7, const_byte_range(instructions, 0x0025));

				// ASSERT
				byte reference2[] = {
					0xE8, 0xB8, 0x53, 0x02, 0x00,
					0x83, 0xC4, 0x0C,
					0x6A, 0x0F,
					0x8D, 0x8D, 0xB4, 0xEF, 0xFF, 0xFF,
					0x51,
					0x8D, 0x95, 0xC5, 0xFE, 0xFF, 0xFF,
					0x52,
					0xE8, 0x70, 0x97, 0x01, 0x00,
					0x83, 0xC4, 0x0C,
					0xE9, 0xF6, 0x32, 0x56, 0x78,
				};

				assert_is_true(equal(reference2, array_end(reference2), translated));
			}


			test( CalculateLengthReturnsSameLengthOnEvenInstructionBoundaries )
			{
				// INIT
				byte instructions[] = {
					0xE8, 0xD4, 0x54, 0x02, 0x00,			// call memset
					0x83, 0xC4, 0x0C,							// add esp, 0Ch
					0x6A, 0x0F,									// push 0Fh
					0x8D, 0x8D, 0xB4, 0xEF, 0xFF, 0xFF,	// lea ecx, [instructions2]
					0x51,											// push ecx
					0x8D, 0x95, 0xC5, 0xFE, 0xFF, 0xFF,	// lea edx, [ebp-13Bh]
					0x52,											// push edx
					0xE8, 0x8C, 0x98, 0x01, 0x00,			// call micro_profiler::move_function
					0x83, 0xC4, 0x0C,							// add esp, 0Ch
					0xE9, 0x12, 0x34, 0x56, 0x78,			// jmp somewhere...
				};

				// ACT / ASSERT
				assert_equal(5u, calculate_function_length(mkrange(instructions), 5));
				assert_equal(3u, calculate_function_length(const_byte_range(instructions + 5, 3), 3));
				assert_equal(2u, calculate_function_length(const_byte_range(instructions + 8, 2), 2));
				assert_equal(8u, calculate_function_length(mkrange(instructions), 8));
				assert_equal(11u, calculate_function_length(const_byte_range(instructions + 5, sizeof(instructions) - 5), 11));
			}


			test( CalculateLengthReturnsCorrectLengthOnUnevenInstructionBoundaries )
			{
				// INIT
				byte instructions[] = {
					0xE8, 0xD4, 0x54, 0x02, 0x00,			// call memset
					0x83, 0xC4, 0x0C,							// add esp, 0Ch
					0x6A, 0x0F,									// push 0Fh
					0x8D, 0x8D, 0xB4, 0xEF, 0xFF, 0xFF,	// lea ecx, [instructions2]
					0x51,											// push ecx
					0x8D, 0x95, 0xC5, 0xFE, 0xFF, 0xFF,	// lea edx, [ebp-13Bh]
					0x52,											// push edx
					0xE8, 0x8C, 0x98, 0x01, 0x00,			// call micro_profiler::move_function
					0x83, 0xC4, 0x0C,							// add esp, 0Ch
					0xE9, 0x12, 0x34, 0x56, 0x78,			// jmp somewhere...
				};

				// ACT / ASSERT
				assert_equal(5u, calculate_function_length(mkrange(instructions), 2));
				assert_equal(3u, calculate_function_length(const_byte_range(instructions + 5, 3), 1));
				assert_equal(8u, calculate_function_length(const_byte_range(instructions + 8, 8), 3));
				assert_equal(8u, calculate_function_length(mkrange(instructions), 8));
				assert_equal(7u, calculate_function_length(const_byte_range(instructions + 16, sizeof(instructions) - 16), 4));
			}


			test( ImagesWithOutsideShortJumpsCannotBeMoved )
			{
				// INIT
				byte instructions[0x0400] = {
					0xEB, 0x02,
					0xEB, 0x00,
					0xEB, 0xF9,
					0xEB, 0xF5,
				};

				// ACT / ASSERT
				assert_throws(move_function(instructions + 0x30, instructions, const_byte_range(instructions, 2)),
					inconsistent_function_range_exception);
			}


			test( ImagesWithInsideShortJumpsCanBeMoved )
			{
				// INIT
				byte instructions[0x0400] = {
					0xEB, 0x02,
					0xEB, 0x00,
					0xEB, 0xFA,
				};

				// ACT
				move_function(instructions + 0x30, instructions, const_byte_range(instructions, 6));

				// ASSERT
				assert_equal(const_byte_range(instructions, 6), const_byte_range(instructions, 6));
			}


			test( SSEInstructionsAreProperlyRead )
			{
				// INIT
				byte is1[] = { 0x66, 0x0F, 0x6C, 0xC0, 0xCC, };
				byte is2[] = { 0x66, 0x0F, 0x7F, 0x44, 0x24, 0x3C, 0xCC, 0xCC, };
				byte is3[] = { 0xF2, 0x0F, 0x70, 0x3C, 0x69, 0x05, }; // pshuflw	xmm7,xmmword ptr [ecx+ebp*2],5
				byte is4[] = { 0x66, 0x0F, 0x71, 0xD0, 0x05, };
				byte is5[] = { 0x0F, 0xC7, 0x09, }; //	cmpxchg8b	qword ptr [ecx]
				byte is6[] = { 0x0F, 0xC7, 0x8C, 0xA9, 0x00, 0x00, 0x10, 0x00, };	//	cmpxchg8b	qword ptr [ecx+ebp*4+100000h]  


				// ACT / ASSERT
				assert_equal(4u, calculate_function_length(mkrange(is1), 1));
				assert_equal(6u, calculate_function_length(mkrange(is2), 1));
				assert_equal(6u, calculate_function_length(mkrange(is3), 1));
				assert_equal(5u, calculate_function_length(mkrange(is4), 1));
				assert_equal(3u, calculate_function_length(mkrange(is5), 1));
				assert_equal(8u, calculate_function_length(mkrange(is6), 1));
			}


			test( ShortJumpsToFragmentMakeCalculateLengthThrowingInconsistencyExceptions )
			{
				throw 0;
			}

		end_test_suite
	}
}