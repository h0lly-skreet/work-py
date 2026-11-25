# Given a non-negative integer x, return the square root of x 
# rounded down to the nearest integer. 
# The returned integer should be non-negative as well.

# You must not use any built-in exponent function or operator.
# For example, do not use pow(x, 0.5) in c++ or x ** 0.5 in python.

class Solution(object):
    def mySqrt(self, x):
        low = 0
        high = x
        while low <= high:
            mid = (low + high) // 2
            square = mid * mid
            if square > x:
                high = mid - 1
            else:
                low = mid + 1
        return low - 1