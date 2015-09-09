/*
 *******************************************************************************
 * Copyright (C) 2015, International Business Machines Corporation and         *
 * others. All Rights Reserved.                                                *
 *******************************************************************************
 */
package com.ibm.icu.dev.test.util;

import java.math.BigDecimal;
import java.math.RoundingMode;

import com.ibm.icu.dev.test.TestFmwk;
import com.ibm.icu.impl.DigitInterval;
import com.ibm.icu.impl.FixedPrecision;
import com.ibm.icu.impl.ScientificPrecision;
import com.ibm.icu.impl.VisibleDigits;
import com.ibm.icu.impl.VisibleDigits.VFixedDecimal;
import com.ibm.icu.impl.VisibleDigitsWithExponent;

/**
 * @author rocketman
 *
 */
public final class VisibleDigitsTest extends TestFmwk {
    /**
    * Constructor
    */
    public VisibleDigitsTest()
    {
    }
      
    // public methods -----------------------------------------------
    
    public static void main(String arg[]) 
    {
        VisibleDigitsTest test = new VisibleDigitsTest();
        try {
            test.run(arg);
        } catch (Exception e) {
            test.errln("Error testing icubinarytest");
        }
    }
    
    public void TestLargeIntValue() {
        VisibleDigits digits;
        {
            FixedPrecision precision = FixedPrecision.DEFAULT;

            // Last 18 digits for int values.
            verifyIFTVHasInt(
                    223372036854775807L, 0L, 0L, 0, true, 
                    precision.initVisibleDigits(Long.MAX_VALUE));
        }
        {
            FixedPrecision precision = new FixedPrecision();
            precision.getMutableMax().setIntDigitCount(5);

            // Last 5 digits for int values.
            digits = precision.initVisibleDigits(Long.MAX_VALUE);
            verifyIFTVHasInt(75807L, 0L, 0L, 0, true, digits);
            verifySource(75807.0, digits);
        }
        {
            FixedPrecision precision = FixedPrecision.DEFAULT;

            // Last 18 digits for int values.
            verifyIFTVHasInt(
                    223372036854775808L, 0L, 0L, 0, true,
                    precision.initVisibleDigits(Long.MIN_VALUE));
        }
        {
            FixedPrecision precision = new FixedPrecision();
            precision.getMutableMax().setIntDigitCount(5);

            // Last 5 digits for int values.
            digits = precision.initVisibleDigits(Long.MIN_VALUE);
            verifyIFTVHasInt(75808L, 0L, 0L, 0, true, digits);
            verifySource(75808.0, digits);
        } 
    }
    
    public void TestIntInitVisibleDigits() {
        verifyVisibleDigits("13", FixedPrecision.DEFAULT.initVisibleDigits(13L));
        verifyVisibleDigits("-17", FixedPrecision.DEFAULT.initVisibleDigits(-17L));
        verifyVisibleDigits("-9223372036854775808", FixedPrecision.DEFAULT.initVisibleDigits(Long.MIN_VALUE));
        verifyVisibleDigits("9223372036854775807", FixedPrecision.DEFAULT.initVisibleDigits(Long.MAX_VALUE));
        verifyVisibleDigits("-31536000", FixedPrecision.DEFAULT.initVisibleDigits(-31536000L));
        verifyVisibleDigits("0", FixedPrecision.DEFAULT.initVisibleDigits(0L));
        {
            FixedPrecision precision = new FixedPrecision();
            precision.getMutableMin().setIntDigitCount(4);
            precision.getMutableMin().setFracDigitCount(2);
            verifyVisibleDigits("0000.00", precision.initVisibleDigits(0L));
            verifyVisibleDigits("0057.00", precision.initVisibleDigits(57L));
            verifyVisibleDigits("-0057.00", precision.initVisibleDigits(-57L));
        }
        {
            FixedPrecision precision = new FixedPrecision();
            precision.getMutableMax().setIntDigitCount(2);
            precision.getMutableMin().setFracDigitCount(1);
            verifyVisibleDigits("35.0", precision.initVisibleDigits(235L));
        }
        {
            FixedPrecision precision = new FixedPrecision();
            precision.getMutableMax().setIntDigitCount(2);
            precision.getMutableMin().setFracDigitCount(1);
            precision.setFailIfOverMax(true);
            try {
                precision.initVisibleDigits(239L);
                fail("expected IllegalArgumentException");
            } catch (IllegalArgumentException expected) {
            }
        }
        {
            FixedPrecision precision = new FixedPrecision();
            precision.getMutableSignificant().setMin(5);
            verifyVisibleDigits("153.00", precision.initVisibleDigits(153L));
        }
        {
            FixedPrecision precision = new FixedPrecision();
            precision.getMutableSignificant().setMax(3);
            verifyVisibleDigits("153", precision.initVisibleDigits(153L));
        }
        {
            FixedPrecision precision = new FixedPrecision();
            precision.getMutableSignificant().setMax(4);
            verifyVisibleDigits("153", precision.initVisibleDigits(153L));
        }
        {
            FixedPrecision precision = new FixedPrecision();
            precision.getMutableSignificant().setMax(2);
            precision.setExactOnly(true);
            try {
                precision.initVisibleDigits(154L);
                fail("expected ArithmeticException");
            } catch (ArithmeticException expected) {
            }
        }
        {
            FixedPrecision precision = new FixedPrecision();
            precision.getMutableSignificant().setMax(2);
            verifyVisibleDigits("150", precision.initVisibleDigits(153L));      
        }       
    }
    
    public void TestIntInitVisibleDigitsToBigDecimal() {
        {
            FixedPrecision precision = new FixedPrecision();
            precision.setRoundingIncrement(new BigDecimal("7.3"));
            verifyVisibleDigits("-29.2", precision.initVisibleDigits(-30L));
        }
        {
            FixedPrecision precision = new FixedPrecision();
            precision.setRoundingIncrement(new BigDecimal("7.3"));
            precision.setRoundingMode(RoundingMode.FLOOR);
            verifyVisibleDigits("-36.5", precision.initVisibleDigits(-30L));
        }
        {
            FixedPrecision precision = new FixedPrecision();
            precision.getMutableSignificant().setMax(3);
            precision.setRoundingMode(RoundingMode.CEILING);
            verifyVisibleDigits("1390", precision.initVisibleDigits(1381L));
        }
        {
            FixedPrecision precision = new FixedPrecision();
            precision.getMutableSignificant().setMax(3);
            precision.setRoundingMode(RoundingMode.FLOOR);
            verifyVisibleDigits("1380", precision.initVisibleDigits(1381L));
        }
        {
            FixedPrecision precision = new FixedPrecision();
            precision.getMutableSignificant().setMax(1);
            precision.setRoundingMode(RoundingMode.FLOOR);
            verifyVisibleDigits("-2000", precision.initVisibleDigits(-1381L));
        }
    }
    
    public void TeestDoubleInitVisibleDigits() {
        verifyVisibleDigits("2.05", FixedPrecision.DEFAULT.initVisibleDigits(2.05));
        verifyVisibleDigits("3547", FixedPrecision.DEFAULT.initVisibleDigits(3547.0));
        verifyVisibleDigits("-2.05", FixedPrecision.DEFAULT.initVisibleDigits(-2.05));
        {
            FixedPrecision precision = new FixedPrecision();
            precision.getMutableMax().setFracDigitCount(2);
            precision.getMutableMax().setIntDigitCount(1);
            precision.setFailIfOverMax(true);
            precision.setExactOnly(true);
            verifyVisibleDigits("-2.05", precision.initVisibleDigits(-2.05));
        }
        {
            FixedPrecision precision = new FixedPrecision();
            precision.getMutableMax().setFracDigitCount(1);
            precision.getMutableMax().setIntDigitCount(1);
            precision.setFailIfOverMax(true);
            precision.setExactOnly(true);
            try {
                precision.initVisibleDigits(-2.05);
                fail("Arithmetic exception expected");
            } catch (ArithmeticException expected) {
            }
        }
        {
            FixedPrecision precision = new FixedPrecision();
            precision.getMutableMax().setFracDigitCount(2);
            precision.getMutableMax().setIntDigitCount(0);
            precision.setFailIfOverMax(true);
            precision.setExactOnly(true);
            try {
                precision.initVisibleDigits(-2.05);
                fail("Illegal argument exception expected");
            } catch (IllegalArgumentException expected) {
            }
        }
        {
            FixedPrecision precision = new FixedPrecision();
            precision.getMutableMin().setFracDigitCount(2);
            precision.getMutableMin().setIntDigitCount(5);
            precision.setExactOnly(true);
            verifyVisibleDigits("06245.30", precision.initVisibleDigits(6245.3));
        }
        {
            FixedPrecision precision = new FixedPrecision();
            precision.getMutableSignificant().setMax(5);
            precision.setExactOnly(true);
            verifyVisibleDigits("6245.3", precision.initVisibleDigits(6245.3));
        }
        {
            FixedPrecision precision = new FixedPrecision();
            precision.getMutableSignificant().setMax(4);
            precision.setExactOnly(true);
            try {
                precision.initVisibleDigits(6245.3);
                fail("Arithmetic exception expected");
            } catch (ArithmeticException expected) {
            }
        }
        {
            FixedPrecision precision = new FixedPrecision();
            precision.getMutableMin().setFracDigitCount(2);
            precision.getMutableMax().setIntDigitCount(3);
            verifyVisibleDigits("384.90", precision.initVisibleDigits(2384.9));
        }
        {
            FixedPrecision precision = new FixedPrecision();
            precision.getMutableMin().setFracDigitCount(2);
            precision.getMutableMax().setIntDigitCount(3);
            precision.setFailIfOverMax(true);
            try {
                precision.initVisibleDigits(2384.9);
                fail("Illegal argument exception expected");
            } catch (IllegalArgumentException expected) {
            }            
        }
    }
    
    public void TestDoubleInitVisibleDigitsToBigDecimal() {
        verifyVisibleDigits("2.01", FixedPrecision.DEFAULT.initVisibleDigits(2.01));
        verifyVisibleDigits("-2.01", FixedPrecision.DEFAULT.initVisibleDigits(-2.01));
        {
            FixedPrecision precision = new FixedPrecision();
            precision.getMutableSignificant().setMax(3);
            precision.getMutableMin().setFracDigitCount(2);
            verifyVisibleDigits("2380.00", precision.initVisibleDigits(2385.0));
        }
        {
            FixedPrecision precision = new FixedPrecision();
            precision.getMutableMax().setFracDigitCount(2);
            verifyVisibleDigits("-45.83", precision.initVisibleDigits(-45.8251));
        }
    }
    
    public void TestSpecialInitVisibleDigits() {
        FixedPrecision precision = new FixedPrecision();
        precision.getMutableSignificant().setMax(3);
        precision.getMutableMin().setFracDigitCount(2);
        {
            VisibleDigits digits = precision.initVisibleDigits(Double.NEGATIVE_INFINITY);
            assertFalse("", digits.isNaN());
            assertTrue("", digits.isInfinite());
            assertTrue("", digits.isNegative());
        }
        {
            VisibleDigits digits = precision.initVisibleDigits(Double.POSITIVE_INFINITY);
            assertFalse("", digits.isNaN());
            assertTrue("", digits.isInfinite());
            assertFalse("", digits.isNegative());
        } 
        {
            VisibleDigits digits = precision.initVisibleDigits(Double.NaN);
            assertTrue("", digits.isNaN());
            assertFalse("", digits.isInfinite());
            assertFalse("", digits.isNegative());
        } 
    }

    public void TestBigDecimalInitVisibleDigits() {
        {
            FixedPrecision precision = new FixedPrecision();
            precision.getMutableMax().setIntDigitCount(3);
            precision.getMutableMin().setFracDigitCount(2);
            precision.setFailIfOverMax(true);
            try {
                precision.initVisibleDigits(new BigDecimal(2384.9));
                fail("Expected illegal argument exception.");
            } catch (IllegalArgumentException expected) {
                // expected
            }
        }
        {
            FixedPrecision precision = new FixedPrecision();
            precision.getMutableSignificant().setMax(4);
            precision.setExactOnly(true);
            try {
                precision.initVisibleDigits(new BigDecimal(6245.3));
                fail("Expected arithmetic exception.");
            } catch (ArithmeticException expected) {
                // expected
            }
        }
    }
    
    public void TestVisibleDigitsWithExponent() {
        verifyVisibleDigitsWithExponent("3.89256E2", ScientificPrecision.DEFAULT.initVisibleDigitsWithExponent(389.256));
        verifyVisibleDigitsWithExponent("-3.89256E2", ScientificPrecision.DEFAULT.initVisibleDigitsWithExponent(-389.256));
        {
            ScientificPrecision precision = new ScientificPrecision();
            precision.setMinExponentDigits(3);
            precision.getMutableMantissa().getMutableMin().setIntDigitCount(1);
            precision.getMutableMantissa().getMutableMax().setIntDigitCount(3);
            verifyVisibleDigitsWithExponent("12.34567E003", precision.initVisibleDigitsWithExponent(12345.67));
        }
        {
            ScientificPrecision precision = new ScientificPrecision();
            precision.getMutableMantissa().setRoundingIncrement(new BigDecimal("0.073"));
            precision.getMutableMantissa().getMutableMin().setIntDigitCount(2);
            precision.getMutableMantissa().getMutableMax().setIntDigitCount(2);
            verifyVisibleDigitsWithExponent("10.001E2", precision.initVisibleDigitsWithExponent(999.74));
        }
    }

    private void verifyIFTVHasInt(long i, long f, long t, int v, boolean hasInt, VisibleDigits digits) {
        VFixedDecimal fd = digits.getFixedDecimal();
        assertEquals("i", i, fd.integerValue);
        assertEquals("f", f, fd.decimalDigits);
        assertEquals("t", t, fd.decimalDigitsWithoutTrailingZeros);
        assertEquals("v", v, fd.visibleDecimalDigitCount);
        assertEquals("hasInt", hasInt, fd.hasIntegerValue);
    }
    
    private void verifySource(double source, VisibleDigits digits) {
        assertEquals("source", source, digits.getFixedDecimal().source);
    }
    
    private void verifyVisibleDigits(String expected, VisibleDigits digits) {
        assertEquals("", expected, digits.toString());
    }
    
    private void verifyVisibleDigitsWithExponent(String expected, VisibleDigitsWithExponent digits) {
        assertEquals("", expected, digits.toString());
    }
    
}