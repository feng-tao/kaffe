/*
 * DomEntityReference.java
 * Copyright (C) 1999,2000,2001 The Free Software Foundation
 * 
 * This file is part of GNU JAXP, a library.
 *
 * GNU JAXP is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * GNU JAXP is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Linking this library statically or dynamically with other modules is
 * making a combined work based on this library.  Thus, the terms and
 * conditions of the GNU General Public License cover the whole
 * combination.
 *
 * As a special exception, the copyright holders of this library give you
 * permission to link this library with independent modules to produce an
 * executable, regardless of the license terms of these independent
 * modules, and to copy and distribute the resulting executable under
 * terms of your choice, provided that you also meet, for each linked
 * independent module, the terms and conditions of the license of that
 * module.  An independent module is a module which is not derived from
 * or based on this library.  If you modify this library, you may extend
 * this exception to your version of the library, but you are not
 * obliged to do so.  If you do not wish to do so, delete this
 * exception statement from your version. 
 */

package gnu.xml.dom;

import org.w3c.dom.*;


/**
 * <p> "EntityReference" implementation (reference to parsed entity).
 * This is a non-core DOM class, supporting the "XML" feature.
 * It does not represent builtin entities (such as "&amp;amp;")
 * or character references, which are always directly expanded in
 * DOM trees.</p>
 *
 * <p> Note that while the DOM specification permits these nodes to have
 * a readonly set of children, this is not required.  Similarly, it does
 * not require a DOM to couple EntityReference nodes with any Entity nodes
 * that have the same entity name (and equivalent children).  It also
 * effectively guarantees that references created directly or indirectly
 * through the <em>Document.ImportNode</em> method will not have children.
 * The level of functionality you may get is extremely variable.
 *
 * <p> Also significant is that even at their most functional level, the fact
 * that EntityReference children must be readonly has caused significant
 * problems when modifying work products held in DOM trees.  Other problems
 * include issues related to undeclared namespace prefixes (and references
 * to the current default namespace) that may be found in the text of such
 * parsed entities nodes.  These must be contextually bound as part of DOM
 * tree construction.  When such nodes are moved, the namespace associated
 * with a given prefix (or default) may change to be in conflict with the
 * namespace bound to the node at creation time.
 *
 * <p> In short, <em>avoid using this DOM functionality</em>.
 *
 * @see DomDoctype
 * @see DomEntity
 *
 * @author David Brownell 
 */
public class DomEntityReference extends DomNode implements EntityReference
{
    private String	name;


    /**
     * Constructs an EntityReference node associated with the specified
     * document.  The creator should populate this with whatever contents
     * are appropriate, and then mark it as readonly.
     *
     * <p>This constructor should only be invoked by a Document as part of
     * its createEntityReference functionality, or through a subclass which
     * is similarly used in a "Sub-DOM" style layer.
     *
     * @see DomNode#makeReadonly
     */
    protected DomEntityReference (Document owner, String name)
    {
	super (owner);
	this.name = name;
    }


    /**
     * <b>DOM L1</b>
     * Returns the name of the referenced entity.
     */
    final public String getNodeName ()
    {
	return name;
    }

    /**
     * <b>DOM L1</b>
     * Returns the constant ENTITY_REFERENCE_NODE.
     */
    final public short getNodeType ()
	{ return ENTITY_REFERENCE_NODE; }
}
