import {
  useEffect, useRef, useState, useImperativeHandle, forwardRef,
} from 'react';

// ── Types ─────────────────────────────────────────────────────────────────────
interface SnakeLinesProps {
  duration?:       number;
  delayBottom?:    number;
  delayMiddle?:    number;
  delayTop?:       number;
  strokeWidth?:    number;
  onOutroFinished?: () => void;
  isNarrow?:       boolean;
}

export interface SnakeLinesHandle {
  startIntroAnimation(): void;
  startOutroAnimation(): void;
}

interface Point { x: number; y: number }

// ── Component ─────────────────────────────────────────────────────────────────
const SnakeLines = forwardRef<SnakeLinesHandle, SnakeLinesProps>(
  function SnakeLines(props, ref) {
    const {
      duration       = 2000,
      delayBottom    = 0,
      delayMiddle    = 200,
      delayTop       = 400,
      onOutroFinished,
      isNarrow       = false,
    } = props;

    const [viewSize, setViewSize] = useState({
      width:  window.innerWidth,
      height: window.innerHeight,
    });

    useEffect(() => {
      const onResize = () =>
        setViewSize({ width: window.innerWidth, height: window.innerHeight });
      window.addEventListener('resize', onResize);
      return () => window.removeEventListener('resize', onResize);
    }, []);

    const { width, height } = viewSize;

    const [tTop, setTTop] = useState(0);
    const [tMid, setTMid] = useState(0);
    const [tBot, setTBot] = useState(0);

    // Use number | null so requestAnimationFrame id and null are both valid
    const introStartRef = useRef<number | null>(null);
    const outroStartRef = useRef<number | null>(null);
    const rafRef        = useRef<number | null>(null);

    const easeInOutQuad = (x: number): number =>
      x < 0.5 ? 2 * x * x : 1 - Math.pow(-2 * x + 2, 2) / 2;

    function cancelAnim() {
      if (rafRef.current !== null) {
        cancelAnimationFrame(rafRef.current);
        rafRef.current = null;
      }
    }

    function startIntroRaf() {
      const total = duration;
      const step = (timestamp: number) => {
        if (introStartRef.current === null) introStartRef.current = timestamp;
        const elapsed = timestamp - introStartRef.current;

        const eBot = Math.max(0, elapsed - delayBottom);
        const eMid = Math.max(0, elapsed - delayMiddle);
        const eTop = Math.max(0, elapsed - delayTop);

        const tBotNorm = Math.min(1, eBot / total);
        const tMidNorm = Math.min(1, eMid / total);
        const tTopNorm = Math.min(1, eTop / total);

        setTBot(easeInOutQuad(tBotNorm));
        setTMid(easeInOutQuad(tMidNorm));
        setTTop(easeInOutQuad(tTopNorm));

        const done = tBotNorm >= 1 && tMidNorm >= 1 && tTopNorm >= 1;
        if (!done) {
          rafRef.current = requestAnimationFrame(step);
        } else {
          rafRef.current = null;
        }
      };
      rafRef.current = requestAnimationFrame(step);
    }

    function startOutroRaf() {
      const total = duration;
      const step = (timestamp: number) => {
        if (outroStartRef.current === null) outroStartRef.current = timestamp;
        const elapsed = timestamp - outroStartRef.current;

        const eBot = Math.max(0, elapsed - delayBottom);
        const eMid = Math.max(0, elapsed - delayMiddle);
        const eTop = Math.max(0, elapsed - delayTop);

        const botNorm = Math.min(1, eBot / total);
        const midNorm = Math.min(1, eMid / total);
        const topNorm = Math.min(1, eTop / total);

        setTTop(1 + easeInOutQuad(topNorm));
        setTMid(1 + easeInOutQuad(midNorm));
        setTBot(1 + easeInOutQuad(botNorm));

        const done = topNorm >= 1 && midNorm >= 1 && botNorm >= 1;
        if (!done) {
          rafRef.current = requestAnimationFrame(step);
        } else {
          rafRef.current = null;
          onOutroFinished?.();
        }
      };
      rafRef.current = requestAnimationFrame(step);
    }

    useImperativeHandle(ref, () => ({
      startIntroAnimation() {
        cancelAnim();
        setTTop(0); setTMid(0); setTBot(0);
        introStartRef.current = null;
        startIntroRaf();
      },
      startOutroAnimation() {
        cancelAnim();
        setTTop(1); setTMid(1); setTBot(1);
        outroStartRef.current = null;
        startOutroRaf();
      },
    }));

    useEffect(() => () => cancelAnim(), []);

    // ── Geometry ──────────────────────────────────────────────────────────────
    const loginWidth      = 320;
    const loginHeight     = 220;
    const loginMarginLeft = 40;

    const loginX0      = loginMarginLeft;
    const loginX1      = loginMarginLeft + loginWidth;
    const loginYCenter = height / 2;
    const loginYTop    = loginYCenter - loginHeight / 2;
    const loginYBot    = loginYCenter + loginHeight / 2;

    const startX = width;
    const startY = height / 3;

    const makeSmoothPath = (yStart: number, yMid: number, yEnd: number): Point[] => {
      const xStart = startX;
      const xMid   = (loginX0 + loginX1) / 2;
      const xEnd   = 0;
      const steps  = 160;
      const pts: Point[] = [];

      for (let i = 0; i <= steps; i++) {
        const t  = i / steps;
        const x1 = xStart + t * (xMid - xStart);
        const x2 = xMid   + t * (xEnd - xMid);
        const x  = x1 + t * (x2 - x1);
        const y1 = yStart + t * (yMid - yStart);
        const y2 = yMid   + t * (yEnd - yMid);
        const y  = y1 + t * (y2 - y1);
        pts.push({ x, y });
      }
      return pts;
    };

    const topYStart = startY - loginHeight * 0.2;
    const topYMid   = loginYTop - loginHeight * 0.3;
    const topYEnd   = loginYTop - loginHeight * 0.2;

    const midYStart = startY;
    const midYMid   = loginYBot + loginHeight * 0.1;
    const midYEnd   = loginYBot + loginHeight * 0.15;

    const botYStart = startY + loginHeight * 0.1;
    const botYMid   = loginYBot + loginHeight * 0.5;
    const botYEnd   = loginYBot + loginHeight * 0.8;

    const pointsTop = makeSmoothPath(topYStart, topYMid, topYEnd);
    const pointsMid = makeSmoothPath(midYStart, midYMid, midYEnd);
    const pointsBot = makeSmoothPath(botYStart, botYMid, botYEnd);

    const toPath = (pts: Point[]): string =>
      pts.reduce(
        (acc: string, p: Point, i: number) =>
          acc + (i === 0 ? `M ${p.x} ${p.y}` : ` L ${p.x} ${p.y}`),
        ''
      );

    const pathTop = toPath(pointsTop);
    const pathMid = toPath(pointsMid);
    const pathBot = toPath(pointsBot);

    const pathLen       = width * 1.5;
    const dashOffsetTop = pathLen * (1.0 - tTop);
    const dashOffsetMid = pathLen * (1.0 - tMid);
    const dashOffsetBot = pathLen * (1.0 - tBot);

    const dynamicStrokeWidth = Math.max(1.5, height * 0.003);
    const maskAttr = isNarrow ? undefined : 'url(#snakeHoleMask)';

    return (
      <svg
        style={{ position: 'fixed', inset: 0, width: '100%', height: '100%', display: 'block' }}
        viewBox={`0 0 ${width} ${height}`}
        preserveAspectRatio="none"
        aria-hidden="true"
      >
        <defs>
          <radialGradient id="snakeGradient" cx="20%" cy="10%" r="120%">
            <stop offset="0%"   stopColor="rgba(245,246,255,1)" />
            <stop offset="100%" stopColor="rgba(202,187,255,1)" />
          </radialGradient>

          <radialGradient id="snakeStrokeRadial" cx="50%" cy="50%" r="80%">
            <stop offset="0%"   stopColor="rgb(223,52,87)"  stopOpacity="1" />
            <stop offset="100%" stopColor="rgb(48,38,194)"  stopOpacity="1" />
          </radialGradient>

          <mask id="snakeHoleMask">
            <rect x="0" y="0" width={width} height={height} fill="white" />
            <linearGradient id="holeGradient" x1="0%" y1="0%" x2="100%" y2="0%">
              <stop offset="0%"   stopColor="black" stopOpacity="0" />
              <stop offset="10%"  stopColor="black" stopOpacity="1" />
              <stop offset="90%"  stopColor="black" stopOpacity="1" />
              <stop offset="100%" stopColor="black" stopOpacity="0" />
            </linearGradient>
            <rect
              x={width * 0.3}
              y={height * 0.3}
              width={width * 0.4}
              height={height * 0.4}
              fill="url(#holeGradient)"
            />
          </mask>
        </defs>

        <rect x="0" y="0" width={width} height={height} fill="url(#snakeGradient)" />

        <g
          fill="none"
          stroke="url(#snakeStrokeRadial)"
          strokeWidth={dynamicStrokeWidth}
          mask={maskAttr}
        >
          <path d={pathTop} strokeDasharray={pathLen} strokeDashoffset={dashOffsetTop} />
          <path d={pathMid} strokeDasharray={pathLen} strokeDashoffset={dashOffsetMid} />
          <path d={pathBot} strokeDasharray={pathLen} strokeDashoffset={dashOffsetBot} />
        </g>
      </svg>
    );
  }
);

export default SnakeLines;
