import random

class Solution(object):
    def sortArray(self, nums):
        if len(nums) < 2:
            return nums
        
        pivot = random.choice(nums)
        left = [x for x in nums if x < pivot]
        middle = [x for x in nums if x == pivot]
        right = [x for x in nums if x > pivot]

        return self.sortArray(left) + middle + self.sortArray(right)