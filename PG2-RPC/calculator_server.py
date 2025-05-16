import grpc
from calculator_pb2_grpc import CalculatorServicer

from calculator_pb2 import SumRequest
from calculator_pb2 import SumReply
from calculator_pb2 import MultiplyRequest
from calculator_pb2 import MultiplyReply
from calculator_pb2 import MaxRequest
from calculator_pb2 import MaxReply
from calculator_pb2 import DivRequest
from calculator_pb2 import DivReply

from grpc import ServicerContext


class Calculator(CalculatorServicer):

    def Sum(self, request: SumRequest, context: ServicerContext) -> SumReply:
        return SumReply(s=request.a + request.b)
    
    def Multiply(self, request: MultiplyRequest, context: ServicerContext) -> MultiplyReply:
        return MultiplyReply(s=request.a * request.b)
    
    def Max(self, request: MaxRequest, context: ServicerContext) -> MaxReply:
        return MaxReply(s=max(request.a, request.b, request.c))
    
    def Div(self, request: DivRequest, context: ServicerContext) -> DivReply:        
        return DivReply(q=int(request.a / request.b), r=request.a % request.b)
    