package com.ibm.text;

import java.util.*;

/**
 * A set of rules for a <code>RuleBasedTransliterator</code>.  This set encodes
 * the transliteration in one direction from one set of characters or short
 * strings to another.  A <code>RuleBasedTransliterator</code> consists of up to
 * two such sets, one for the forward direction, and one for the reverse.
 *
 * <p>A <code>TransliterationRuleSet</code> has one important operation, that of
 * finding a matching rule at a given point in the text.  This is accomplished
 * by the <code>findMatch()</code> method.
 *
 * <p>Copyright &copy; IBM Corporation 1999.  All rights reserved.
 *
 * @author Alan Liu
 * @version $RCSfile: TransliterationRuleSet.java,v $ $Revision: 1.2 $ $Date: 1999/12/22 00:01:36 $
 *
 * $Log: TransliterationRuleSet.java,v $
 * Revision 1.2  1999/12/22 00:01:36  Alan
 * Detect a>x masking a>y
 *
 */
class TransliterationRuleSet {
    /* Note: There was an old implementation that indexed by first letter of
     * key.  Problem with this is that key may not have a meaningful first
     * letter; e.g., {Lu}>*.  One solution is to keep a separate vector of all
     * rules whose intial key letter is a category variable.  However, the
     * problem is that they must be kept in order with respect to other rules.
     * One solution -- add a sequence number to each rule.  Do the usual
     * first-letter lookup, and also a lookup from the spare bin with rules like
     * {Lu}>*.  Take the lower sequence number.  This seems complex and not
     * worth the trouble, but we may revisit this later.  For documentation (or
     * possible resurrection) the old code is included below, commented out
     * with the remark "// OLD INDEXED IMPLEMENTATION".  Under the old
     * implementation, <code>rules</code> is a Hashtable, not a Vector.
     */

    /**
     * Vector of rules, in the order added.
     */
    private Vector rules;

    /**
     * Length of the longest preceding context
     */
    private int maxContextLength;

    private static final String COPYRIGHT =
        "\u00A9 IBM Corporation 1999. All rights reserved.";

    /**
     * Construct a new empty rule set.
     */
    public TransliterationRuleSet() {
        rules = new Vector();
        maxContextLength = 0;
    }

    /**
     * Return the maximum context length.
     * @return the length of the longest preceding context.
     */
    public int getMaximumContextLength() {
        return maxContextLength;
    }

    /**
     * Add a rule to this set.  Rules are added in order, and order is
     * significant.
     *
     * <p>Once freeze() is called, this method must not be called.
     * @param rule the rule to add
     */
    public void addRule(TransliterationRule rule) {
        
        // Build time, no checking  : 3562 ms
        // Build time, with checking: 6234 ms

        for (int i=0; i<rules.size(); ++i) {
            TransliterationRule r = (TransliterationRule) rules.elementAt(i);
            if (r.masks(rule)) {
                throw new IllegalArgumentException("Rule " + r +
                                                   " masks " + rule);
            }
        }

        rules.addElement(rule);
        int len;
        if ((len = rule.getAnteContextLength()) > maxContextLength) {
            maxContextLength = len;
        }
    }

    /**
     * Free up space.  Once this method is called, addRule() must NOT
     * be called again.
     */
    public void freeze() {
        for (int i=0; i<rules.size(); ++i) {
            ((TransliterationRule) rules.elementAt(i)).freeze();
        }
    }

    /**
     * Attempt to find a matching rule at the specified point in the text.  The
     * text being matched occupies a virtual buffer consisting of the contents
     * of <code>result</code> concatenated to a substring of <code>text</code>.
     * The substring is specified by <code>start</code> and <code>limit</code>.
     * The value of <code>cursor</code> is an index into this virtual buffer,
     * from 0 to the length of the buffer.  In terms of the parameters,
     * <code>cursor</code> must be between 0 and <code>result.length() + limit -
     * start</code>.
     * @param text the untranslated text
     * @param start the beginning index, inclusive; <code>0 <= start
     * <= limit</code>.
     * @param limit the ending index, exclusive; <code>start <= limit
     * <= text.length()</code>.
     * @param result tranlated text
     * @param cursor position at which to translate next, an offset into result.
     * If greater than or equal to result.length(), represents offset start +
     * cursor - result.length() into text.
     * @param variables a dictionary mapping variables to the sets they
     * represent (maps <code>Character</code> to <code>UnicodeSet</code>)
     * @param filter the filter.  Any character for which
     * <tt>filter.isIn()</tt> returns <tt>false</tt> will not be
     * altered by this transliterator.  If <tt>filter</tt> is
     * <tt>null</tt> then no filtering is applied.
     * @return the matching rule, or null if none found.
     */
    public TransliterationRule findMatch(String text, int start, int limit,
                                         StringBuffer result, int cursor,
                                         Dictionary variables,
                                         UnicodeFilter filter) {
        for (Enumeration e = rules.elements(); e.hasMoreElements(); ) {
            TransliterationRule rule = (TransliterationRule) e.nextElement();
            if (rule.matches(text, start, limit, result, cursor, variables, filter)) {
                return rule;
            }
        }
        return null;
    }

    /**
     * Attempt to find a matching rule at the specified point in the text.
     * @param text the text, both translated and untranslated
     * @param start the beginning index, inclusive; <code>0 <= start
     * <= limit</code>.
     * @param limit the ending index, exclusive; <code>start <= limit
     * <= text.length()</code>.
     * @param cursor position at which to translate next, representing offset
     * into text.  This value must be between <code>start</code> and
     * <code>limit</code>.
     * @param variables a dictionary mapping variables to the sets they
     * represent (maps <code>Character</code> to <code>UnicodeSet</code>)
     * @param filter the filter.  Any character for which
     * <tt>filter.isIn()</tt> returns <tt>false</tt> will not be
     * altered by this transliterator.  If <tt>filter</tt> is
     * <tt>null</tt> then no filtering is applied.
     * @return the matching rule, or null if none found.
     */
    public TransliterationRule findMatch(Replaceable text, int start, int limit,
                                         int cursor,
                                         Dictionary variables,
                                         UnicodeFilter filter) {
        for (Enumeration e = rules.elements(); e.hasMoreElements(); ) {
            TransliterationRule rule = (TransliterationRule) e.nextElement();
            if (rule.matches(text, start, limit, cursor, variables, filter)) {
                return rule;
            }
        }
        return null;
    }

    /**
     * Attempt to find a matching rule at the specified point in the text.
     * Unlike <code>findMatch()</code>, this method does an incremental match.
     * An incremental match requires that there be no partial matches that might
     * pre-empt the full match that is found.  If there are partial matches,
     * then null is returned.  A non-null result indicates that a full match has
     * been found, and that it cannot be pre-empted by a partial match
     * regardless of what additional text is added to the translation buffer.
     * @param text the text, both translated and untranslated
     * @param start the beginning index, inclusive; <code>0 <= start
     * <= limit</code>.
     * @param limit the ending index, exclusive; <code>start <= limit
     * <= text.length()</code>.
     * @param cursor position at which to translate next, representing offset
     * into text.  This value must be between <code>start</code> and
     * <code>limit</code>.
     * @param variables a dictionary mapping variables to the sets they
     * represent (maps <code>Character</code> to <code>UnicodeSet</code>)
     * @param partial output parameter.  <code>partial[0]</code> is set to
     * true if a partial match is returned.
     * @param filter the filter.  Any character for which
     * <tt>filter.isIn()</tt> returns <tt>false</tt> will not be
     * altered by this transliterator.  If <tt>filter</tt> is
     * <tt>null</tt> then no filtering is applied.
     * @return the matching rule, or null if none found, or if the text buffer
     * does not have enough text yet to unambiguously match a rule.
     */
    public TransliterationRule findIncrementalMatch(Replaceable text, int start,
                                                    int limit, int cursor,
                                                    Dictionary variables,
                                                    boolean partial[],
                                                    UnicodeFilter filter) {
        partial[0] = false;
        for (Enumeration e = rules.elements(); e.hasMoreElements(); ) {
            TransliterationRule rule = (TransliterationRule) e.nextElement();
            int match = rule.getMatchDegree(text, start, limit, cursor,
                                            variables, filter);
            switch (match) {
            case TransliterationRule.FULL_MATCH:
                return rule;
            case TransliterationRule.PARTIAL_MATCH:
                partial[0] = true;
                return null;
            }
        }
        return null;
    }
}
