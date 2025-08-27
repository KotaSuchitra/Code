/**
 * Definition for a binary tree node.
 * struct TreeNode {
 *     int val;
 *     struct TreeNode *left;
 *     struct TreeNode *right;
 * };
 */
/**
 * Note: The returned array must be malloced, assume caller calls free().
 */
void inorder(struct TreeNode *root, int *res, int *size)
{
    if (root != NULL)
    {
        inorder(root->left, res, size);
        res[(*size)++] = root->val;
        inorder(root->right, res, size);
    }
}

int *inorderTraversal(struct TreeNode *root, int *returnSize)
{
    int *res = (int *)malloc(sizeof(int) * 100);
    int size = 0;
    inorder(root, res, &size);
    *returnSize = size;
    return res;
}
