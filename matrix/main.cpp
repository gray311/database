#include<bits/stdc++.h>
#include"p0_starter.h"
using namespace std;
using namespace scudb;
int main()
{
    RowMatrixOperations<int> test;
    RowMatrix<int> temp(3,4);
    cout<<temp.GetRowCount()<<endl;
    cout<<endl;
    vector<int>q;
    for(int i=1;i<=12;++i) q.push_back(i);
    temp.FillFrom(q);
    temp.Print();
    cout<<endl;
    std::unique_ptr<RowMatrix<int> > p1 = test.Add( &temp, &temp);
    p1->Print();
    cout<<"--------------------------"<<endl;

    RowMatrix<int> tmp(4,3);
    cout<<temp.GetRowCount()<<endl;
    cout<<endl;
    vector<int>t;
    for(int i=1;i<=12;++i) t.push_back(i);
    tmp.FillFrom(t);
    tmp.Print();
    cout<<endl;
    std::unique_ptr<RowMatrix<int> > p2 = test.Multiply(&temp,&tmp);
    p2->Print();
    cout<<"--------------------------"<<endl;

    RowMatrix<int> tp(3,3);
    cout<<tp.GetRowCount()<<endl;
    cout<<endl;
    vector<int>p;
    for(int i=1;i<=9;++i) p.push_back(i);
    tp.FillFrom(p);
    tp.Print();
    cout<<endl;
    std::unique_ptr<RowMatrix<int> > p3 = test.GEMM(&temp,&tmp,&tp);
    p3->Print();

    system("pause");
    return 0;
}