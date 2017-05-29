# os-team20

## Project 4, Finally!

## High-Level Design

## Implementation

## Permission Policy
Since linux doesn't support floating point operations inside kernel, we used following formulas to compare file location with current location(GPS hardware location).

```
Suppose x is physical distance(in km).
With R = 6400km(earth radius) and theta(angle in degrees),
x = R * theta*(360/2pi)
  
When theta is 0.000001 (in other words, fractional part of degree equals to 1)
x = 6400 * 0.000001 * 360 / 2pi
  = 0.111701m
  
This means that small change in integer part of degree becomes significantly large when converted to distance. In this case we can assume that two locations are far enough from each other.
Thus, our kernel
1. Check if integer parts of latitude and longitude are equal.
2. If equal, checks if distance calculated from fractional parts, is less than sum of two accuracy.
```

## Lessons Learned
