from calculator_pb2_grpc import CalculatorServicer

from calculator_pb2 import SumRequest
from calculator_pb2 import SumReply
from calculator_pb2 import MultiplyRequest
from calculator_pb2 import MultiplyReply
from calculator_pb2 import MaxRequest
from calculator_pb2 import MaxReply

from grpc import ServicerContext


class Calculator(CalculatorServicer):

    def Sum(self, request: SumRequest, context: ServicerContext) -> SumReply:
        return SumReply(s=request.a + request.b)
    
    def Multiply(self, request: MultiplyRequest, context: ServicerContext) -> MultiplyReply:
        return MultiplyReply(s=request.a * request.b)
    
    def Max(self, request: MaxRequest, context: ServicerContext) -> MaxReply:
        return SumReply(s=max(request.a, request.b, request.c))
