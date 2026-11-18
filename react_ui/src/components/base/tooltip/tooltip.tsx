import React, { useState, useRef, useEffect, ReactNode, cloneElement } from "react";
import { createPortal } from "react-dom";

export interface TooltipProps {
  title: ReactNode;
  description?: ReactNode;
  children: React.ReactElement<any>;
  arrow?: boolean;
  delay?: number;
  placement?: "top" | "bottom" | "left" | "right";
  offset?: number;
  className?: string;
}

/**
 * Untitled UI Floating Tooltip
 * Works on any child element (native button, div, a, etc.) with zero setup.
 * Features:
 * - Direct portal rendering to document.body (never clipped by cards or overflow)
 * - Untitled UI Dark 900 styling (#0C111D) with soft drop shadow
 * - Smart boundary auto-detection
 */
export const Tooltip = ({
  title,
  description,
  children,
  arrow = true,
  delay = 180,
  placement = "top",
  offset = 8,
  className = ""
}: TooltipProps) => {
  const [isOpen, setIsOpen] = useState(false);
  const [coords, setCoords] = useState<{ top: number; left: number }>({ top: 0, left: 0 });
  const triggerRef = useRef<HTMLElement | null>(null);
  const tooltipRef = useRef<HTMLDivElement | null>(null);
  const timeoutRef = useRef<any>(null);

  const updatePosition = () => {
    if (!triggerRef.current) return;
    const rect = triggerRef.current.getBoundingClientRect();
    const tooltipEl = tooltipRef.current;
    const ttWidth = tooltipEl ? tooltipEl.offsetWidth : 160;
    const ttHeight = tooltipEl ? tooltipEl.offsetHeight : 34;

    let top = 0;
    let left = 0;

    if (placement === "top") {
      top = rect.top - ttHeight - offset;
      left = rect.left + rect.width / 2 - ttWidth / 2;
    } else if (placement === "bottom") {
      top = rect.bottom + offset;
      left = rect.left + rect.width / 2 - ttWidth / 2;
    } else if (placement === "left") {
      top = rect.top + rect.height / 2 - ttHeight / 2;
      left = rect.left - ttWidth - offset;
    } else if (placement === "right") {
      top = rect.top + rect.height / 2 - ttHeight / 2;
      left = rect.right + offset;
    }

    // Boundary constraints so tooltip never clips outside screen
    const pad = 8;
    if (left < pad) left = pad;
    if (left + ttWidth > window.innerWidth - pad) {
      left = window.innerWidth - pad - ttWidth;
    }
    if (top < pad && placement === "top") {
      top = rect.bottom + offset;
    }

    setCoords({ top, left });
  };

  const isTouchDevice = () => {
    if (typeof window === "undefined") return false;
    return (
      "ontouchstart" in window ||
      navigator.maxTouchPoints > 0 ||
      window.matchMedia("(pointer: coarse)").matches ||
      window.innerWidth <= 768
    );
  };

  const handleMouseEnter = () => {
    if (isTouchDevice()) return;
    clearTimeout(timeoutRef.current);
    timeoutRef.current = setTimeout(() => {
      setIsOpen(true);
    }, delay);
  };

  const handleMouseLeave = () => {
    clearTimeout(timeoutRef.current);
    setIsOpen(false);
  };

  useEffect(() => {
    if (isOpen) {
      updatePosition();
      const timer = setTimeout(updatePosition, 10);
      const handleScrollOrResize = () => updatePosition();
      window.addEventListener("scroll", handleScrollOrResize, true);
      window.addEventListener("resize", handleScrollOrResize);
      return () => {
        clearTimeout(timer);
        window.removeEventListener("scroll", handleScrollOrResize, true);
        window.removeEventListener("resize", handleScrollOrResize);
      };
    }
  }, [isOpen]);

  useEffect(() => {
    return () => clearTimeout(timeoutRef.current);
  }, []);

  if (!React.isValidElement(children)) {
    return <>{children}</>;
  }

  const trigger = cloneElement(children as React.ReactElement<any>, {
    ref: (node: any) => {
      triggerRef.current = node;
      const { ref } = children as any;
      if (typeof ref === "function") ref(node);
      else if (ref) ref.current = node;
    },
    onMouseEnter: (e: any) => {
      handleMouseEnter();
      if (children.props && children.props.onMouseEnter) children.props.onMouseEnter(e);
    },
    onMouseOver: (e: any) => {
      handleMouseEnter();
      if (children.props && children.props.onMouseOver) children.props.onMouseOver(e);
    },
    onMouseLeave: (e: any) => {
      handleMouseLeave();
      if (children.props && children.props.onMouseLeave) children.props.onMouseLeave(e);
    },
    onFocus: (e: any) => {
      if (isTouchDevice()) return;
      handleMouseEnter();
      if (children.props && children.props.onFocus) children.props.onFocus(e);
    },
    onTouchStart: (e: any) => {
      clearTimeout(timeoutRef.current);
      setIsOpen(false);
      if (children.props && children.props.onTouchStart) children.props.onTouchStart(e);
    },
    onBlur: (e: any) => {
      handleMouseLeave();
      if (children.props && children.props.onBlur) children.props.onBlur(e);
    }
  });

  return (
    <>
      {trigger}
      {isOpen &&
        typeof document !== "undefined" &&
        createPortal(
          <div
            ref={tooltipRef}
            role="tooltip"
            className={`untitledui-tooltip-box ${className}`}
            style={{
              position: "fixed",
              top: `${coords.top}px`,
              left: `${coords.left}px`,
              zIndex: 999999,
              pointerEvents: "none"
            }}
          >
            <span className="untitledui-tooltip-title">{title}</span>
            {description && <span className="untitledui-tooltip-desc">{description}</span>}
          </div>,
          document.body
        )}
    </>
  );
};

export const TooltipTrigger = ({ children }: { children: React.ReactNode }) => <>{children}</>;
